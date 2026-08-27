// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/session_lock_state/session_lock_state.h"

#include <QObject>
#include <QString>
#include <QtTypes>

namespace QindaQt::Services::SessionLockState {

// Asynchronous platform seam used by SessionLockStateMonitor. Implementations
// must never resolve a well-known service into a cached owner: every request
// asks the bus daemon, and lock signals are bound to the supplied unique owner.
// The transport is borrowed by the monitor, must outlive it, and shares its
// QObject affinity thread.
class SessionLockTransport : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~SessionLockTransport() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;

    virtual void requestServiceOwner(quint64 generation,
                                     ObservedService service) = 0;
    virtual void requestUnixProcessId(quint64 generation,
                                      const QString &uniqueOwner) = 0;

    // Subscription is a local bus-match installation, not a blocking remote
    // call. A successful return guarantees both signals are subscribed before
    // the monitor is permitted to call requestActiveState().
    [[nodiscard]] virtual bool subscribeToLockSignals(
        const QString &uniqueOwner) = 0;
    virtual void unsubscribeFromLockSignals() = 0;

    virtual void requestActiveState(quint64 generation, quint64 serial,
                                    const QString &uniqueOwner) = 0;
    virtual void scheduleActiveRetry(quint64 generation, quint64 serial,
                                     int delayMilliseconds) = 0;

Q_SIGNALS:
    // Emitted once after the implementation has stopped and torn down its
    // runtime resources because the underlying bus connection was lost.
    // Consumers must revoke previously authenticated state; owner-change
    // signals are not a reliable substitute when the daemon itself disappears.
    void transportLost();
    void serviceOwnerChanged(ObservedService service,
                             const QString &uniqueOwner);
    void serviceOwnerResolved(quint64 generation, ObservedService service,
                              const QString &uniqueOwner);
    void unixProcessIdResolved(quint64 generation, const QString &uniqueOwner,
                               quint64 processId);
    void activeStateResolved(quint64 generation, quint64 serial,
                             const QString &uniqueOwner, bool active);
    void requestFailed(quint64 generation, quint64 serial,
                       LockRequest request, ObservedService service,
                       const QString &uniqueOwner, const QString &errorName,
                       const QString &message);
    void aboutToLock(const QString &uniqueOwner);
    void activeChanged(const QString &uniqueOwner, bool active);
    void activeRetryReady(quint64 generation, quint64 serial);
};

} // namespace QindaQt::Services::SessionLockState
