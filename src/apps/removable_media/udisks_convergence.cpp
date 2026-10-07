// SPDX-License-Identifier: GPL-3.0-or-later
#include "udisks_backend.h"
#include <QDBusArgument>

namespace QindaQt::Apps::RemovableMedia {
namespace Public = QindaQt::RemovableMedia;
namespace {
const QString Block = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString Filesystem = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString Drive = QStringLiteral("org.freedesktop.UDisks2.Drive");
const QString Encrypted = QStringLiteral("org.freedesktop.UDisks2.Encrypted");
}
void UDisksBackend::setPhase(Public::ProgressPhase phase)
{
    m_phase = phase;
    Q_EMIT changed();
}
bool UDisksBackend::siblingsReleased() const
{
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
        const auto block = it.value().value(Block);
        QString drive = objectPath(block.value(QStringLiteral("Drive")));
        if (drive == QStringLiteral("/")) {
            const auto backing = objectPath(block.value(QStringLiteral("CryptoBackingDevice")));
            drive = objectPath(m_objects.value(QDBusObjectPath(backing)).value(Block).value(QStringLiteral("Drive")));
        }
        if (drive != m_expected.drive) continue;
        if (!qdbus_cast<QList<QByteArray>>(it.value().value(Filesystem).value(QStringLiteral("MountPoints"))).isEmpty())
            return false;
        const auto cleartext = objectPath(it.value().value(Encrypted).value(QStringLiteral("CleartextDevice")));
        if (!cleartext.isEmpty() && cleartext != QStringLiteral("/")) return false;
    }
    return true;
}
void UDisksBackend::confirmBeforeRemoval()
{
    if (!m_request || m_converging) return;
    m_converging = true;
    setPhase(Public::ProgressPhase::Refreshing);
    m_convergenceTimer.start();
    const auto serial = m_mutationSerial;
    fetch([this, serial](bool success) {
        if (!m_request || serial != m_mutationSerial) return;
        m_convergenceTimer.stop();
        m_converging = false;
        const auto currentDrive = m_objects.value(QDBusObjectPath(m_expected.drive)).value(Drive);
        if (!success || currentDrive.isEmpty()
            || physicalMediaIdentity(m_expected.drive, currentDrive) != m_expected.driveIdentity || !siblingsReleased()) {
            complete(false, QStringLiteral("Could not confirm every volume was unmounted and locked. Check the drive before trying again."),
                     {}, Public::OperationStatus::Uncertain);
            return;
        }
        m_preFinalConfirmed = true;
        runNext();
    });
}
void UDisksBackend::converge(const QString &message, const QString &mountPath)
{
    if (!m_request || m_converging) return;
    m_converging = true;
    setPhase(Public::ProgressPhase::Refreshing);
    m_convergenceTimer.start();
    const auto serial = m_mutationSerial;
    fetch([this, serial, message, mountPath](bool success) {
        if (!m_request || serial != m_mutationSerial) return;
        const auto *current = find(m_request->token);
        bool confirmed = success && current && current->identity == m_expected.identity;
        if (m_request->operation == Operation::Remove) {
            const auto drive = m_objects.value(QDBusObjectPath(m_expected.drive)).value(Drive);
            confirmed = success && !drive.isEmpty()
                && physicalMediaIdentity(m_expected.drive, drive) == m_expected.driveIdentity && siblingsReleased();
            if (confirmed) m_removalMode = Public::RemovalMode::Unmounted;
        } else if (m_request->operation == Operation::Unmount) {
            confirmed = confirmed && current->mountRoots.isEmpty();
        } else {
            confirmed = confirmed && !current->mountRoots.isEmpty() && current->mountRoots.contains(mountPath);
            if (m_request->operation == Operation::MountReadOnly)
                confirmed = confirmed && current->readOnlyKnown && current->readOnly;
        }
        complete(confirmed, confirmed ? message
            : QStringLiteral("The disk request completed, but its current state could not be confirmed. Refresh before trying again."),
            confirmed ? mountPath : QString{}, Public::OperationStatus::Uncertain);
    });
}
}
