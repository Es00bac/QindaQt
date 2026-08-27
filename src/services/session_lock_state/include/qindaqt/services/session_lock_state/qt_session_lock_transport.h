// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/session_lock_state/session_lock_transport.h"

#include <QDBusConnection>
#include <QList>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::Services::SessionLockState {

class QtSessionLockTransport final : public SessionLockTransport {
    Q_OBJECT

public:
    explicit QtSessionLockTransport(
        QDBusConnection connection = QDBusConnection::sessionBus(),
        QObject *parent = nullptr);
    ~QtSessionLockTransport() override;

    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    void requestServiceOwner(quint64 generation,
                             ObservedService service) override;
    void requestUnixProcessId(quint64 generation,
                              const QString &uniqueOwner) override;
    [[nodiscard]] bool subscribeToLockSignals(
        const QString &uniqueOwner) override;
    void unsubscribeFromLockSignals() override;
    void requestActiveState(quint64 generation, quint64 serial,
                            const QString &uniqueOwner) override;
    void scheduleActiveRetry(quint64 generation, quint64 serial,
                             int delayMilliseconds) override;

private Q_SLOTS:
    void handleBusDisconnected();
    void handleAboutToLock();
    void handleActiveChanged(bool active);

private:
    void clearRuntimeResources();
    void track(QDBusPendingCallWatcher *watcher);
    void finish(QDBusPendingCallWatcher *watcher);
    void emitFailure(quint64 generation, quint64 serial,
                     LockRequest request, ObservedService service,
                     const QString &uniqueOwner, const QString &errorName,
                     const QString &message);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QList<QDBusPendingCallWatcher *> m_pendingCalls;
    QString m_signalOwner;
    quint64 m_lifetimeGeneration = 0;
    bool m_disconnectSubscribed = false;
    bool m_started = false;
};

} // namespace QindaQt::Services::SessionLockState
