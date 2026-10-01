// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <QDBusConnection>
#include <QTimer>
namespace QindaQt::Services::Portal {
// Helper-private joining of the actual inherited ordinary UNIX peer, retained
// PIDFD, current bus owner/PID and nonce-authenticated native lock observer.
// No frontend-supplied lock bit, owner string alone or bare UID admits pixels.
class NativeCaptureAdmission final : public QObject {
    Q_OBJECT
public:
    NativeCaptureAdmission(QDBusConnection bus, QString expectedOwner, int borrowedPeerFd);
    ~NativeCaptureAdmission() override;
    bool admitted() const;
    bool lineageLive() const;
Q_SIGNALS:
    void ready();
    void lost();
private:
    bool identityLive(const QString &owner, quint64 pid) const;
    QDBusConnection m_bus; QString m_owner; quint64 m_pid = 0; int m_pidfd = -1;
    bool m_once = false;
    SessionLockState::QtNativeLockTransport m_transport;
    SessionLockState::NativeLockStateMonitor m_monitor;
    QTimer m_lifetime;
};
}
