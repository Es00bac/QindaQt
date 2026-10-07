// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/removable_media_client/media_client.h>
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <utility>

namespace QindaQt::RemovableMedia {
namespace {
QString service() { return QString::fromLatin1(kServiceName); }
QString path() { return QString::fromLatin1(kObjectPath); }
QString interface() { return QString::fromLatin1(kInterfaceName); }
}
MediaClient::MediaClient(QDBusConnection bus, MediaOwnerLauncher &launcher, QObject *parent)
    : MediaSource(parent), m_bus(std::move(bus)), m_launcher(launcher),
      m_watcher(service(), m_bus, QDBusServiceWatcher::WatchForOwnerChange)
{
    m_startupTimer.setSingleShot(true);
    m_startupTimer.setInterval(5000);
    connect(&m_startupTimer, &QTimer::timeout, this, [this] {
        m_launchPending = false;
        publishUnavailable(DiagnosticCode::Unavailable,
            QStringLiteral("Media support did not become ready. Try again or open Removable Media from the launcher."));
    });
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &owner) {
            if (m_started) setOwner(owner);
        });
}
MediaClient::~MediaClient()
{
    if (!m_owner.isEmpty())
        m_bus.disconnect(m_owner, path(), interface(), QStringLiteral("SnapshotChanged"),
                         this, SLOT(changedWire(QByteArray,QDBusMessage)));
}
void MediaClient::start()
{
    if (m_started) return;
    m_started = true;
    auto query = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("GetNameOwner"));
    query.setArguments({service()});
    query.setAutoStartService(false);
    const auto serial = m_ownerSerial;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(query, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, serial](QDBusPendingCallWatcher *pending) {
            const QDBusPendingReply<QString> reply = *pending;
            pending->deleteLater();
            if (serial != m_ownerSerial) return;
            setOwner(reply.isValid() ? reply.value() : QString{});
        });
}
void MediaClient::publishUnavailable(DiagnosticCode code, const QString &message)
{
    Snapshot empty;
    empty.availability = Availability::Unavailable;
    empty.diagnostic = {code, message};
    m_snapshot = std::move(empty);
    Q_EMIT snapshotChanged();
}
void MediaClient::setOwner(const QString &owner)
{
    if (!m_owner.isEmpty())
        m_bus.disconnect(m_owner, path(), interface(), QStringLiteral("SnapshotChanged"),
                         this, SLOT(changedWire(QByteArray,QDBusMessage)));
    ++m_ownerSerial;
    ++m_readSerial;
    m_owner = owner;
    m_snapshot = {};
    m_observed = {};
    m_retiredEpochs.clear();
    if (owner.isEmpty()) {
        publishUnavailable(DiagnosticCode::Unavailable,
            QStringLiteral("Media support unavailable. Start media support to show devices."));
        return;
    }
    // AGENT-GUARD: unique destination + captured serial fence every reply.
    // Observing a browser must not activate the owner's remembered-mount policy.
    m_bus.connect(owner, path(), interface(), QStringLiteral("SnapshotChanged"),
                  this, SLOT(changedWire(QByteArray,QDBusMessage)));
    Q_EMIT snapshotChanged();
    requestSnapshot();
}
void MediaClient::refresh()
{
    if (!m_started) { start(); return; }
    if (m_owner.isEmpty()) {
        publishUnavailable(DiagnosticCode::Unavailable,
            QStringLiteral("Media support unavailable. Start media support to show devices."));
    } else requestSnapshot();
}
void MediaClient::requestSnapshot()
{
    if (m_owner.isEmpty()) return;
    auto query = QDBusMessage::createMethodCall(m_owner, path(), interface(),
                                               QStringLiteral("GetSnapshot"));
    query.setAutoStartService(false);
    const auto serial = ++m_readSerial;
    const auto ownerSerial = m_ownerSerial;
    const auto owner = m_owner;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(query, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, serial, ownerSerial, owner](QDBusPendingCallWatcher *pending) {
            const QDBusPendingReply<QByteArray> reply = *pending;
            pending->deleteLater();
            if (ownerSerial != m_ownerSerial || serial != m_readSerial || owner != m_owner) return;
            if (!reply.isValid()) {
                const auto error = reply.error().type();
                const bool old = error == QDBusError::UnknownObject || error == QDBusError::UnknownMethod
                    || error == QDBusError::UnknownInterface;
                publishUnavailable(old ? DiagnosticCode::Unsupported : DiagnosticCode::Unavailable,
                    old ? QStringLiteral("This media owner has no device inventory. Open Removable Media.")
                        : QStringLiteral("Could not read media support. Try again."));
                return;
            }
            acceptWire(reply.value(), owner, serial);
        });
}
void MediaClient::acceptWire(const QByteArray &wire, const QString &owner, quint64 serial)
{
    if (owner != m_owner || serial != m_readSerial) return;
    Snapshot next;
    if (!decodeSnapshot(wire, next).succeeded() || next.lineage.owner != owner) {
        publishUnavailable(DiagnosticCode::Invalid, QStringLiteral("Media support sent an invalid inventory. Try again."));
        return;
    }
    if (m_retiredEpochs.contains(next.lineage.epoch)) {
        publishUnavailable(DiagnosticCode::Stale, QStringLiteral("The media authority changed. Try again."));
        return;
    }
    if (m_observed.lineage.epoch == next.lineage.epoch && !next.lineage.epoch.isEmpty()) {
        if (next.lineage.revision < m_observed.lineage.revision
            || (next.lineage.revision == m_observed.lineage.revision && next != m_observed)) {
            publishUnavailable(DiagnosticCode::Invalid, QStringLiteral("Media inventory changed without a current revision. Try again."));
            return;
        }
        if (next == m_snapshot) return;
    } else if (!m_observed.lineage.epoch.isEmpty()) {
        if (m_retiredEpochs.size() >= 64) {
            publishUnavailable(DiagnosticCode::Invalid, QStringLiteral("Media support changed too often. Reopen this browser."));
            return;
        }
        m_retiredEpochs.append(m_observed.lineage.epoch);
    }
    m_observed = next;
    m_snapshot = std::move(next);
    if (m_snapshot.availability == Availability::Ready) {
        m_startupTimer.stop();
        m_launchPending = false;
    }
    Q_EMIT snapshotChanged();
}
void MediaClient::changedWire(const QByteArray &wire, const QDBusMessage &message)
{
    if (message.service() != m_owner) return;
    // Retire an older pending read; a signal cannot be rolled back by its reply.
    acceptWire(wire, m_owner, ++m_readSerial);
}
void MediaClient::recover()
{
    if (!m_started) start();
    if (!m_owner.isEmpty()) {
        if (m_snapshot.diagnostic.code == DiagnosticCode::Unsupported) openOwner();
        else refresh();
        return;
    }
    if (m_launchPending) return;
    m_launchPending = true;
    m_snapshot = {};
    Q_EMIT snapshotChanged();
    if (!m_launcher.startOwner()) {
        m_launchPending = false;
        publishUnavailable(DiagnosticCode::Unavailable,
            QStringLiteral("Could not start media support. Try again or open Removable Media from the launcher."));
    } else m_startupTimer.start();
}
void MediaClient::openOwner()
{
    if (m_owner.isEmpty()) { recover(); return; }
    auto request = QDBusMessage::createMethodCall(m_owner,
        QStringLiteral("/org/qindaqt/RemovableMedia1"), service(), QStringLiteral("Activate"));
    request.setAutoStartService(false);
    m_bus.asyncCall(request, 5000);
}
} // namespace QindaQt::RemovableMedia
