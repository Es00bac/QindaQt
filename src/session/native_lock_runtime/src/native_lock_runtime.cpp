#include <qindaqt/session/native_lock_runtime/native_lock_runtime.h>
#include <qindaqt/services/power_client/power_client.h>
#include <algorithm>
namespace QindaQt::Session::NativeLockRuntime {
using Services::SessionLockState::LockState;
Runtime::Runtime(Services::LockPreferences::PreferencesProvider &preferences,
                 Platform::Idle::IdleObservation &idle,
                 Services::NativeLock::NativeLockRequest &request,
                 Services::SessionLockState::NativeLockStateMonitor &state,
                 Power::PowerClient *power, QObject *parent)
    : QObject(parent), m_preferences(preferences), m_idle(idle),
      m_request(request), m_state(state), m_power(power) {
  m_grace.setSingleShot(true);
  m_suspendDeadline.setSingleShot(true);
  connect(&m_grace, &QTimer::timeout, this, &Runtime::graceExpired);
  connect(&m_suspendDeadline, &QTimer::timeout, this, [this] { finishSuspend(false); });
  connect(&m_preferences, &Services::LockPreferences::PreferencesProvider::changed,
          this, &Runtime::refreshPreferences);
  connect(&m_idle, &Platform::Idle::IdleObservation::changed, this, &Runtime::idleChanged);
  connect(&m_request, &Services::NativeLock::NativeLockRequest::completed, this,
          [this](const auto result) {
    m_requestPending = false;
    Q_EMIT lockFinished(result);
    if (result != Services::NativeLock::RequestResult::Admitted && m_suspendDispatch)
      finishSuspend(false);
    Q_EMIT stateChanged();
  });
  connect(&m_state, &Services::SessionLockState::NativeLockStateMonitor::protectionChanged,
          this, &Runtime::nativeProtectionChanged);
  connect(&m_state, &Services::SessionLockState::NativeLockStateMonitor::stateChanged,
          this, &Runtime::nativeStateChanged);
  if (m_power) {
    connect(m_power, &Power::PowerClient::snapshotChanged, this, &Runtime::powerSnapshotChanged);
    connect(m_power, &Power::PowerClient::stateChanged, this, &Runtime::powerSnapshotChanged);
    connect(m_power, &Power::PowerClient::idleInhibitorStateChanged, this,
            &Runtime::idleChanged);
  }
}
Runtime::~Runtime() { stop(); }
bool Runtime::start() {
  if (m_started) return true;
  m_started = true;
  refreshPreferences();
  m_idle.refresh();
  if (m_power) m_power->start();
  m_state.refresh();
  m_status = QStringLiteral("native lock runtime active");
  Q_EMIT stateChanged();
  idleChanged();
  return true;
}
void Runtime::stop() {
  if (!m_started) return;
  m_started = false;
  m_grace.stop();
  m_suspendDeadline.stop();
  m_suspendDispatch = {};
  m_idle.setTimeout(0);
  m_idle.revoke();
  if (m_power) m_power->stop();
  m_state.stop();
  m_values.reset();
  m_status = QStringLiteral("native lock runtime stopped");
  Q_EMIT stateChanged();
}
bool Runtime::available() const noexcept {
  return m_started && m_values.has_value() && m_idle.available();
}
QString Runtime::status() const { return m_status; }
void Runtime::refreshPreferences() {
  m_values = m_preferences.preferences();
  if (!m_values || !m_values->lockOnResume) m_resumeLockPending = false;
  m_grace.stop();
  m_automaticCycleRequested = false;
  if (m_values && m_values->automaticLock) {
    const qint64 ms = std::min<qint64>(m_values->idleTimeoutSeconds * 1000,
                                       24 * 60 * 60 * 1000LL);
    m_idle.setTimeout(static_cast<int>(ms));
  } else {
    m_idle.setTimeout(0);
  }
  m_status = m_values ? QStringLiteral("native lock preferences confirmed")
                      : QStringLiteral("waiting for confirmed lock preferences");
  Q_EMIT stateChanged();
  idleChanged();
}
void Runtime::idleChanged() {
  m_grace.stop();
  if (!m_started || !m_values || !m_values->automaticLock ||
      !m_idle.available() || !m_idle.idle()) {
    m_automaticCycleRequested = false;
    return;
  }
  // A live Power1 owner may already hold accepted leases while its snapshot
  // query is pending or has failed. Do not race that unknown state by firing
  // an automatic lock stage; owner disappearance revokes those leases.
  if (m_power && !m_power->owner().isEmpty() &&
      !m_power->hasIdleInhibitorState()) {
    m_automaticCycleRequested = false;
    m_status = QStringLiteral("automatic lock waiting for current idle lease state");
    Q_EMIT stateChanged();
    return;
  }
  if (m_power && m_power->hasIdleInhibitorState() &&
      m_power->activeIdleInhibitorScopes().testFlag(
          Power::IdleInhibitorScope::AutomaticLock)) {
    m_automaticCycleRequested = false;
    m_status = QStringLiteral("automatic lock deferred by confirmed idle lease");
    Q_EMIT stateChanged();
    return;
  }
  if (m_automaticCycleRequested) return;
  const int delay = static_cast<int>(std::clamp<qint64>(m_values->graceSeconds * 1000, 0, 300000));
  if (delay == 0) graceExpired(); else m_grace.start(delay);
}
void Runtime::graceExpired() {
  if (!m_started || !m_values || !m_values->automaticLock ||
      !m_idle.available() || !m_idle.idle() || m_automaticCycleRequested) return;
  m_automaticCycleRequested = acquireForReason(QStringLiteral("idle"));
}
bool Runtime::acquireForReason(const QString &reason) {
  Q_UNUSED(reason);
  if (!m_started || m_requestPending || m_state.state() != LockState::Unlocked) {
    m_status = QStringLiteral("native lock state unavailable or request busy");
    Q_EMIT stateChanged();
    return false;
  }
  m_requestPending = m_request.request();
  m_status = m_requestPending ? QStringLiteral("native lock admission pending")
                              : QStringLiteral("native lock request unavailable");
  Q_EMIT stateChanged();
  return m_requestPending;
}
bool Runtime::requestManualLock() {
  if (!m_started) return false;
  if (m_state.state() == LockState::Locked || m_state.state() == LockState::Locking) return true;
  return acquireForReason(QStringLiteral("manual"));
}
bool Runtime::requestSuspend(std::function<void()> dispatch) {
  if (!m_started || !dispatch || m_suspendDispatch) return false;
  if (m_state.presentationProtected()) {
    dispatch();
    Q_EMIT suspendDispatchFinished(true);
    return true;
  }
  m_suspendDispatch = std::move(dispatch);
  m_suspendDeadline.start(15000);
  if (m_state.state() == LockState::Unlocked) {
    if (!acquireForReason(QStringLiteral("suspend"))) finishSuspend(false);
  } else if (m_state.state() == LockState::Unknown || m_state.state() == LockState::Locking) {
    m_state.refresh();
  } else {
    finishSuspend(false);
  }
  return bool(m_suspendDispatch);
}
void Runtime::nativeProtectionChanged() {
  if (m_suspendDispatch && m_state.presentationProtected()) finishSuspend(true);
}
void Runtime::nativeStateChanged() {
  Q_EMIT stateChanged();
  if (m_resumeLockPending && m_values && m_values->lockOnResume) {
    if (m_state.state() == LockState::Unlocked) {
      m_resumeLockPending = false;
      static_cast<void>(acquireForReason(QStringLiteral("resume")));
    } else if (m_state.state() == LockState::Locked ||
               m_state.state() == LockState::Locking) {
      m_resumeLockPending = false;
    }
  }
  if (m_suspendDispatch && m_state.state() == LockState::Unlocked &&
      !m_requestPending) {
    if (!acquireForReason(QStringLiteral("suspend"))) finishSuspend(false);
    return;
  }
  if (m_started && m_idle.idle() && !m_automaticCycleRequested) idleChanged();
}
void Runtime::finishSuspend(const bool protectedNow) {
  if (!m_suspendDispatch) return;
  m_suspendDeadline.stop();
  auto dispatch = std::move(m_suspendDispatch);
  m_suspendDispatch = {};
  if (protectedNow) dispatch();
  Q_EMIT suspendDispatchFinished(protectedNow);
}
void Runtime::powerSnapshotChanged() {
  if (!m_power || !m_power->hasSnapshot()) return;
  const bool preparing = m_power->snapshot().source.preparingForSleep;
  if (m_preparingForSleep && !preparing && m_values && m_values->lockOnResume) {
    m_resumeLockPending = true;
    nativeStateChanged();
  }
  m_preparingForSleep = preparing;
}
}
