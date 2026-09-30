#pragma once
#include <QObject>
#include <QTimer>
#include <functional>
#include <optional>
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/lock_preferences/lock_preferences.h>
#include <qindaqt/services/native_lock_service/native_lock_request.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
namespace QindaQt::Power { class PowerClient; }
namespace QindaQt::Session::NativeLockRuntime {
class Runtime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available NOTIFY stateChanged)
  Q_PROPERTY(QString status READ status NOTIFY stateChanged)
public:
  Runtime(Services::LockPreferences::PreferencesProvider &preferences,
          Platform::Idle::WaylandIdleObservation &idle,
          Services::NativeLock::NativeLockRequest &request,
          Services::SessionLockState::NativeLockStateMonitor &state,
          Power::PowerClient *power, QObject *parent = nullptr);
  ~Runtime() override;
  bool start();
  void stop();
  bool available() const noexcept;
  QString status() const;
  bool requestManualLock();
  // Dispatches only after an actual Protected receipt; cannot control a
  // privileged external logind caller.
  bool requestSuspend(std::function<void()> dispatch);
Q_SIGNALS:
  void stateChanged();
  void lockFinished(QindaQt::Services::NativeLock::RequestResult result);
  void suspendDispatchFinished(bool dispatched);
private:
  void refreshPreferences();
  void idleChanged();
  void graceExpired();
  void powerSnapshotChanged();
  void nativeProtectionChanged();
  void nativeStateChanged();
  bool acquireForReason(const QString &reason);
  void finishSuspend(bool protectedNow);
  Services::LockPreferences::PreferencesProvider &m_preferences;
  Platform::Idle::WaylandIdleObservation &m_idle;
  Services::NativeLock::NativeLockRequest &m_request;
  Services::SessionLockState::NativeLockStateMonitor &m_state;
  Power::PowerClient *m_power;
  QTimer m_grace;
  QTimer m_suspendDeadline;
  std::optional<Services::LockPreferences::Preferences> m_values;
  std::function<void()> m_suspendDispatch;
  QString m_status;
  bool m_started = false;
  bool m_preparingForSleep = false;
  bool m_resumeLockPending = false;
  bool m_requestPending = false;
  bool m_automaticCycleRequested = false;
};
}
