// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QTimer>
#include <qindaqt/session/native_sleep/logind_sleep_transport.h>
#include <qindaqt/session/native_lock_runtime/native_lock_runtime.h>
namespace QindaQt::Session::NativeSleep {
// Same-thread borrowed collaborators must outlive this coordinator. No unlock
// authority: logind Unlock is observed but cannot authenticate the native lock.
// A delay inhibitor is finite logind courtesy, never a veto of external sleep.
class SleepCoordinator final : public QObject {
  Q_OBJECT
public:
  SleepCoordinator(LogindSleepTransport &transport,
      NativeLockRuntime::Runtime &runtime,
      Services::SessionLockState::NativeLockStateMonitor &state,
      QObject *parent = nullptr);
  ~SleepCoordinator() override;
  void start();
  void stop();
  bool canSuspend() const;
  bool requestSuspend();
Q_SIGNALS:
  void availabilityChanged();
  void suspendFinished(bool confirmed);
private:
  void preparing(bool preparing);
  void authorityChanged();
  void syncLockedHint();
  void finishManual(bool confirmed);
  LogindSleepTransport &m_transport;
  NativeLockRuntime::Runtime &m_runtime;
  Services::SessionLockState::NativeLockStateMonitor &m_state;
  QTimer m_deadline;
  quint64 m_serial = 0;
  bool m_started = false, m_manual = false, m_sleepGate = false;
};
}
