// SPDX-License-Identifier: GPL-3.0-or-later
#include "udisks_backend.h"
#include <QDBusArgument>
#include <QDBusReply>
#include <QDir>
#include <QStorageInfo>

namespace QindaQt::Apps::RemovableMedia {
namespace {
const QString Block = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString Filesystem = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString Drive = QStringLiteral("org.freedesktop.UDisks2.Drive");
const QString Encrypted = QStringLiteral("org.freedesktop.UDisks2.Encrypted");
}
void UDisksBackend::execute(const Request &request)
{
    const auto *v = find(request.token);
    if (m_request || !m_available || !v) {
        Q_EMIT finished(request.token, false, QStringLiteral("This media is unavailable or another operation is running."), {});
        return;
    }
    m_request = request;
    m_expected = *v;
    m_resultMount.clear();
    m_unlockedRemovalTransition = false;
    m_converging = m_preFinalConfirmed = false;
    m_removalDisappearanceSeen = m_removalReplacementSeen = false;
    m_removalMode = QindaQt::RemovableMedia::RemovalMode::None;
    setPhase(QindaQt::RemovableMedia::ProgressPhase::Confirming);
    const auto serial = ++m_mutationSerial;
    // AGENT-GUARD: confirmation captures an attachment, not /dev/sdX. Read
    // current UDisks state immediately before admission, and never replay
    // requests after owner loss, unplug or an uncertain result.
    fetch([this, serial](bool success) {
        if (!m_request || serial != m_mutationSerial) return;
        if (!success) { finish(false, QStringLiteral("Could not confirm the current media. Refresh before trying again."), {}, QindaQt::RemovableMedia::OperationStatus::Uncertain); return; }
        const auto *current = find(m_request->token);
        if (!current || current->identity != m_expected.identity) {
            finish(false, QStringLiteral("The media changed before the operation could start."), {}, QindaQt::RemovableMedia::OperationStatus::Gone);
            return;
        }
        m_expected = *current;
        prepare(*m_request, *current);
    });
}
void UDisksBackend::prepare(const Request &request, const Volume &v)
{
    const QVariantMap options;
    const auto add = [this](const QString &path, const QString &interface,
                            const QString &method, const QVariantList &arguments) {
        m_steps.enqueue({path, interface, method, arguments});
    };
    switch (request.operation) {
    case Operation::Mount:
    case Operation::MountReadOnly: {
        const bool ro = request.operation == Operation::MountReadOnly || v.readOnly;
        if (!v.mountable || (ro && !v.readOnly && !v.canMountReadOnly)) {
            finish(false, QStringLiteral("Read-only options cannot be applied to this system-configured mount, or this media has no mountable filesystem."));
            return;
        }
        if (!v.mountPath.isEmpty()) {
            if (ro && !v.readOnly) finish(false, QStringLiteral("This media is already mounted writable. Unmount it before choosing read-only."));
            else finish(true, QStringLiteral("This media is already mounted."), v.mountPath);
            return;
        }
        QVariantMap mountOptions;
        if (ro) mountOptions.insert(QStringLiteral("options"), QStringLiteral("ro"));
        add(v.path, Filesystem, QStringLiteral("Mount"), {mountOptions});
        break;
    }
    case Operation::Unmount:
        if (v.mountPath.isEmpty()) { finish(true, QStringLiteral("This media is already unmounted.")); return; }
        add(v.path, Filesystem, QStringLiteral("Unmount"), {options});
        break;
    case Operation::Remove: {
        // Include hidden sibling filesystems: safely removing a disk must not
        // leave a recovery/hidden partition mounted by another application.
        // No force option: a busy volume remains mounted with a visible error.
        for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
            const auto block = it.value().value(Block);
            QString drive = objectPath(block.value(QStringLiteral("Drive")));
            if (drive == QStringLiteral("/")) {
                const auto backing = objectPath(block.value(QStringLiteral("CryptoBackingDevice")));
                drive = objectPath(m_objects.value(QDBusObjectPath(backing)).value(Block).value(QStringLiteral("Drive")));
            }
            if (drive != v.drive) continue;
            const auto mounts = qdbus_cast<QList<QByteArray>>(it.value().value(Filesystem).value(QStringLiteral("MountPoints")));
            if (!mounts.isEmpty()) add(it.key().path(), Filesystem, QStringLiteral("Unmount"), {options});
        }
        for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
            if (objectPath(it.value().value(Block).value(QStringLiteral("Drive"))) != v.drive) continue;
            const auto cleartext = objectPath(it.value().value(Encrypted).value(QStringLiteral("CleartextDevice")));
            if (!cleartext.isEmpty() && cleartext != QStringLiteral("/"))
                add(it.key().path(), Encrypted, QStringLiteral("Lock"), {options});
        }
        if (v.canEject) add(v.drive, Drive, QStringLiteral("Eject"), {options});
        else if (v.canPowerOff) add(v.drive, Drive, QStringLiteral("PowerOff"), {options});
        break;
    }
    case Operation::Format: {
        const int maxLabel = request.filesystem == QStringLiteral("vfat") ? 11 : 16;
        if (!v.canFormat || !v.mountPath.isEmpty() || !m_formatTypes.contains(request.filesystem)
            || request.label.toUtf8().size() > maxLabel || request.label.contains(QChar::Null)
            || request.label.contains(QLatin1Char('/'))) {
            finish(false, QStringLiteral("Formatting requires unmounted writable media, an available filesystem, and a short label without slashes."));
            return;
        }
        // Do not tear down mounts or nested structures implicitly. Formatting
        // applies only to the confirmed volume, never its parent whole disk.
        QVariantMap formatOptions{{QStringLiteral("label"), request.label},
            {QStringLiteral("take-ownership"), true}, {QStringLiteral("update-partition-type"), true}};
        add(v.path, Block, QStringLiteral("Format"), {request.filesystem, formatOptions});
        break;
    }
    case Operation::Unlock:
        if (!v.encrypted || request.passphrase.isEmpty() || request.passphrase.size() > 4096) {
            finish(false, QStringLiteral("Enter the passphrase for an encrypted volume.")); return;
        }
        add(v.path, Encrypted, QStringLiteral("Unlock"), {request.passphrase, options});
        break;
    }
    // Keep passphrase lifetime bounded to the single outstanding D-Bus call.
    m_request->passphrase.clear();
    runNext();
}
void UDisksBackend::runNext()
{
    if (!m_request) return;
    if (m_request->operation == Operation::Remove && m_removalReplacementSeen) {
        finish(false, QStringLiteral("The attachment changed during safe removal. Refresh before trying again."), {},
               QindaQt::RemovableMedia::OperationStatus::Uncertain);
        return;
    }
    if (m_steps.isEmpty()) {
        const QString message = m_request->operation == Operation::Remove ? QStringLiteral("Media can now be safely removed.")
            : m_request->operation == Operation::Unmount ? QStringLiteral("Media unmounted.")
            : m_request->operation == Operation::Format ? QStringLiteral("Media formatted. You can now mount it.")
            : m_request->operation == Operation::Unlock ? QStringLiteral("Volume unlocked. Choose Mount to access its files.")
            : QStringLiteral("Media mounted%1.").arg(m_request->operation == Operation::MountReadOnly ? QStringLiteral(" read-only") : QString{});
        finish(true, message, m_resultMount);
        return;
    }
    const auto currentDrive = m_objects.value(QDBusObjectPath(m_expected.drive)).value(Drive);
    const bool sameDrive = !currentDrive.isEmpty()
        && physicalMediaIdentity(m_expected.drive, currentDrive) == m_expected.driveIdentity;
    const bool stillPresent = m_request->operation == Operation::Remove
        ? sameDrive && (find(m_request->token) != nullptr || m_unlockedRemovalTransition)
        : find(m_request->token) != nullptr;
    if (!stillPresent) { finish(false, QStringLiteral("The media was removed during the operation."), {}, QindaQt::RemovableMedia::OperationStatus::Gone); return; }
    const Step &next = m_steps.head();
    if (m_request->operation == Operation::Remove && !m_preFinalConfirmed
        && (next.method == QStringLiteral("Eject") || next.method == QStringLiteral("PowerOff"))) {
        confirmBeforeRemoval();
        return;
    }
    const Step step = m_steps.dequeue();
    setPhase(step.method == QStringLiteral("Mount") ? QindaQt::RemovableMedia::ProgressPhase::Mounting
        : step.method == QStringLiteral("Unmount") ? QindaQt::RemovableMedia::ProgressPhase::Unmounting
        : step.method == QStringLiteral("Lock") ? QindaQt::RemovableMedia::ProgressPhase::Locking
        : step.method == QStringLiteral("Eject") ? QindaQt::RemovableMedia::ProgressPhase::Ejecting
        : step.method == QStringLiteral("PowerOff") ? QindaQt::RemovableMedia::ProgressPhase::PoweringOff
        : QindaQt::RemovableMedia::ProgressPhase::Confirming);
    const auto serial = m_mutationSerial;
    call(step, [this, serial, method = step.method, path = step.path](const QDBusMessage &reply) {
        if (!m_request || serial != m_mutationSerial) return;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            QString message;
            if (reply.errorName().endsWith(QStringLiteral("DeviceBusy"))) message = QStringLiteral("This media is busy. Close files and applications using it, then try again.");
            else if (reply.errorName().contains(QStringLiteral("NotAuthorized"))) message = QStringLiteral("The operation was not authorized or authentication was cancelled.");
            else if (m_request->operation == Operation::Unlock) message = QStringLiteral("Could not unlock this volume. Check the passphrase and try again.");
            else message = QStringLiteral("The disk service could not complete the operation: ") + reply.errorMessage().left(700);
            const auto name = reply.errorName();
            const auto failure = name.endsWith(QStringLiteral("DeviceBusy")) ? QindaQt::RemovableMedia::OperationStatus::Busy
                : name.contains(QStringLiteral("Dismissed")) || name.contains(QStringLiteral("Cancelled"))
                    ? QindaQt::RemovableMedia::OperationStatus::Cancelled
                : name.contains(QStringLiteral("NoReply")) || name.contains(QStringLiteral("Timeout"))
                    || name.contains(QStringLiteral("Disconnected")) ? QindaQt::RemovableMedia::OperationStatus::Uncertain
                : QindaQt::RemovableMedia::OperationStatus::Refused;
            finish(false, message, {}, failure);
            return;
        }
        if (method == QStringLiteral("Eject") || method == QStringLiteral("PowerOff")) {
            const auto currentDrive = m_objects.value(QDBusObjectPath(m_expected.drive)).value(Drive);
            if (m_removalReplacementSeen || (!currentDrive.isEmpty() && physicalMediaIdentity(m_expected.drive, currentDrive) != m_expected.driveIdentity)) {
                finish(false, QStringLiteral("The drive changed before safe removal was confirmed."), {},
                       QindaQt::RemovableMedia::OperationStatus::Uncertain);
                return;
            }
            m_removalMode = method == QStringLiteral("Eject") ? QindaQt::RemovableMedia::RemovalMode::Ejected
                : QindaQt::RemovableMedia::RemovalMode::PoweredOff;
        }
        if (method == QStringLiteral("Mount")) {
            const auto *current = find(m_request->token);
            if (!current || current->identity != m_expected.identity) {
                finish(false, QStringLiteral("The media changed while mounting. The old reply was ignored."));
                return;
            }
            const QDBusReply<QString> mountReply(reply);
            if (!mountReply.isValid() || !QDir::isAbsolutePath(mountReply.value())) {
                finish(false, QStringLiteral("The disk service did not return a mount location. Refresh to check its state."));
                return;
            }
            m_resultMount = mountReply.value();
            if (m_request->operation == Operation::MountReadOnly && !m_expected.readOnly) {
                QStorageInfo storage(m_resultMount);
                if (!storage.isValid() || !storage.isReady() || !storage.isReadOnly()) {
                    // System fstab can override requested mount options. Do
                    // not announce writable storage as a read-only success.
                    const Step rollback{m_expected.path, Filesystem, QStringLiteral("Unmount"), {QVariantMap{}}};
                    call(rollback, [this, serial](const QDBusMessage &unmountReply) {
                        if (!m_request || serial != m_mutationSerial) return;
                        finish(false, unmountReply.type() == QDBusMessage::ErrorMessage
                            ? QStringLiteral("Read-only mounting could not be verified, and unmounting failed. This media may still be mounted writable; check More options.")
                            : QStringLiteral("Read-only mounting could not be verified. The media was unmounted."));
                    });
                    return;
                }
            }
        }
        // Only our Lock of this selected cleartext child's backing volume
        // may remove its token while the remaining physical removal proceeds.
        if (method == QStringLiteral("Lock") && path == m_expected.cryptoBackingDevice)
            m_unlockedRemovalTransition = true;
        runNext();
    });
}
void UDisksBackend::finish(bool success, const QString &message, const QString &mountPath,
                            QindaQt::RemovableMedia::OperationStatus failure)
{
    if (!m_request) return;
    if (success && (m_request->operation == Operation::Mount || m_request->operation == Operation::MountReadOnly
        || m_request->operation == Operation::Unmount
        || (m_request->operation == Operation::Remove && m_removalMode == QindaQt::RemovableMedia::RemovalMode::None))) {
        converge(message, mountPath);
        return;
    }
    complete(success, message, mountPath, failure);
}
void UDisksBackend::complete(bool success, const QString &message, const QString &mountPath,
                            QindaQt::RemovableMedia::OperationStatus failure)
{
    if (!m_request) return;
    const QString token = m_request->token;
    const auto operation = m_request->operation;
    const BackendCompletion result{token, operation,
        success ? QindaQt::RemovableMedia::OperationStatus::Applied : failure,
        success ? m_removalMode : QindaQt::RemovableMedia::RemovalMode::None, message, mountPath};
    m_request.reset();
    ++m_mutationSerial;
    m_steps.clear();
    m_convergenceTimer.stop();
    m_converging = false;
    m_phase = QindaQt::RemovableMedia::ProgressPhase::Idle;
    // AGENT-CONTRACT: typed completion follows authoritative readback/final
    // UDisks success; the public exporter must never infer it from a UI string.
    Q_EMIT operationCompleted(result);
    Q_EMIT finished(token, success, message, mountPath);
    Q_EMIT changed();
    refresh();
}
} // namespace QindaQt::Apps::RemovableMedia
