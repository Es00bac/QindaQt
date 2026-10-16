// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "qindaqt/services/session_lock_state/session_lock_state.h"
#include <functional>
#include <qindaqt/platform/recovery_backoff/recovery_backoff.h>
namespace QindaQt::Services::SessionLockState {
class NativeLockTransport;
// Retry schedule after a failed, slow or refused probe/query. Production uses
// the defaults; deterministic tests inject manual timers.
struct NativeLockMonitorRecovery final {
  Platform::Recovery::BackoffPolicy policy{};
  Platform::Recovery::RecoveryTimerFactory timers{};
};
// AGENT-CONTRACT: PID comes only from the bus daemon, never a native-client
// payload. Admission must consult an independently trusted live attachment,
// including its exact owner/PID. It is synchronous, read-only and same-thread;
// captures must outlive the monitor. Empty or revoked admission fails closed.
using NativeLockAdmission =
    std::function<bool(const QString &uniqueOwner, quint64 daemonResolvedPid)>;
class NativeLockStateMonitor final : public QObject {
  Q_OBJECT
  Q_PROPERTY(LockState state READ state NOTIFY stateChanged)
  Q_PROPERTY(bool contentMayBeShown READ contentMayBeShown NOTIFY
                 contentMayBeShownChanged)
  Q_PROPERTY(bool presentationProtected READ presentationProtected NOTIFY
                 protectionChanged)
public:
  NativeLockStateMonitor(NativeLockTransport &borrowed,
                         NativeLockAdmission admission,
                         QObject *parent = nullptr);
  NativeLockStateMonitor(NativeLockTransport &borrowed,
                         NativeLockAdmission admission,
                         NativeLockMonitorRecovery recovery,
                         QObject *parent = nullptr);
  ~NativeLockStateMonitor() override;
  bool start(QString *error = nullptr);
  void stop();
  // The attachment owner calls refresh() on admission changes/revocation.
  // Getters also recheck admission, so an event-loop delay cannot admit
  // content.
  // AGENT-CONTRACT (ADR-0379): every failed, timed-out or refused probe or
  // query publishes Unknown and retries with bounded backoff (0.25 s doubling
  // to 30 s) while started; a retry never supplies state, only a fresh
  // nonce receipt does. refresh() retries immediately and restarts the
  // schedule, so an owner that re-attaches leaves Unknown at once.
  void refresh();
  LockState state() const;
  bool contentMayBeShown() const { return state() == LockState::Unlocked; }
  bool presentationProtected() const;
Q_SIGNALS:
  void stateChanged(LockState state);
  void contentMayBeShownChanged(bool contentMayBeShown);
  void protectionChanged(bool protectedPresentation);

private:
  void probe();
  void query();
  void invalidate();
  void retryLater();
  void recover();
  void publish(LockState state, bool protectedPresentation);
  bool admitted() const;
  NativeLockTransport &m_transport;
  NativeLockAdmission m_admission;
  Platform::Recovery::RecoveryBackoff m_recovery;
  QString m_owner;
  quint64 m_pid = 0, m_generation = 0, m_serial = 0;
  LockState m_state = LockState::Unknown;
  bool m_protected = false, m_started = false, m_authenticated = false;
};
} // namespace QindaQt::Services::SessionLockState
