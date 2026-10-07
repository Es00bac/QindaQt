// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_client/media_source.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QTimer>

namespace QindaQt::RemovableMedia {
// Exact-owner, bounded asynchronous Devices-v1 transport. The registered
// connection and injected launcher outlive this same-thread client. Raw D-Bus
// errors never reach presentation. Calls/notifications cannot rebind a stale
// callback to a replacement owner. No bus activation or mutation in observation.
class MediaClient final : public MediaSource {
    Q_OBJECT
public:
    MediaClient(QDBusConnection connection, MediaOwnerLauncher &launcher,
                QObject *parent = nullptr);
    ~MediaClient() override;
    [[nodiscard]] Snapshot snapshot() const override { return m_snapshot; }
    void start() override;
    void refresh() override;
    void recover() override;
    void openOwner() override;
private Q_SLOTS:
    void changedWire(const QByteArray &wire, const QDBusMessage &message);
private:
    void setOwner(const QString &owner);
    void requestSnapshot();
    void publishUnavailable(DiagnosticCode code, const QString &message);
    void acceptWire(const QByteArray &wire, const QString &owner, quint64 serial);
    QDBusConnection m_bus;
    MediaOwnerLauncher &m_launcher;
    QDBusServiceWatcher m_watcher;
    QTimer m_startupTimer, m_readTimer;
    Snapshot m_snapshot, m_observed;
    QStringList m_retiredEpochs;
    QString m_owner;
    quint64 m_ownerSerial = 0, m_readSerial = 0;
    bool m_started = false, m_launchPending = false;
};
} // namespace QindaQt::RemovableMedia
