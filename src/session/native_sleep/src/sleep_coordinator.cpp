// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/native_sleep/sleep_coordinator.h>
#include <QPointer>
namespace QindaQt::Session::NativeSleep {
using Services::SessionLockState::LockState;
SleepCoordinator::SleepCoordinator(LogindSleepTransport &transport,
    NativeLockRuntime::Runtime &runtime,
    Services::SessionLockState::NativeLockStateMonitor &state, QObject *parent)
    : QObject(parent), m_transport(transport), m_runtime(runtime), m_state(state) {
  m_deadline.setSingleShot(true);
  m_deadline.setInterval(17000);
  connect(&m_deadline, &QTimer::timeout, this, [this] {
    ++m_serial; m_runtime.cancelSuspend(); refuseManual();
  });
  connect(&m_transport, &LogindSleepTransport::availabilityChanged,
          this, &SleepCoordinator::authorityChanged);
  connect(&m_transport, &LogindSleepTransport::lockRequested, this, [this] {
    if (m_started) static_cast<void>(m_runtime.requestManualLock());
  });
  // AGENT-GUARD: login1 Unlock is not PAM approval and never calls native unlock.
  connect(&m_transport, &LogindSleepTransport::unlockRequested, this, [this] {
    if (m_started) m_state.refresh();
  });
  connect(&m_transport, &LogindSleepTransport::prepareForSleep,
          this, &SleepCoordinator::preparing);
  connect(&m_transport, &LogindSleepTransport::suspendFinished,
          this, &SleepCoordinator::finishManual);
  connect(&m_state, &Services::SessionLockState::NativeLockStateMonitor::stateChanged,
          this, &SleepCoordinator::syncLockedHint);
  connect(&m_state, &Services::SessionLockState::NativeLockStateMonitor::protectionChanged,
          this, &SleepCoordinator::syncLockedHint);
  connect(&m_runtime, &NativeLockRuntime::Runtime::suspendDispatchFinished,
          this, [this](bool dispatched) { if (!dispatched) refuseManual(); });
}
SleepCoordinator::~SleepCoordinator() { stop(); }
void SleepCoordinator::start() {
  if (m_started) return;
  m_started = true;
  m_transport.start();
  syncLockedHint();
}
void SleepCoordinator::stop() {
  m_started = false; ++m_serial;
  m_sleepGate = false;
  m_runtime.cancelSuspend();
  refuseManual();
  m_transport.stop();
}
bool SleepCoordinator::canSuspend() const {
  const auto state = m_state.state();
  return m_started && !m_manual && !m_transport.preparingForSleep() &&
         m_transport.hasDelayInhibitor() &&
         (state == LockState::Unlocked ||
          (state == LockState::Locked && m_state.presentationProtected()));
}
void SleepCoordinator::queryCapability(SleepMode mode, std::function<void(bool)> completion) {
  if (!canSuspend()) { completion(false); return; }
  const auto serial = m_serial;
  m_transport.queryCapability(mode, [self = QPointer<SleepCoordinator>(this), serial,
      completion = std::move(completion)](bool capable) {
    if (self) completion(capable && serial == self->m_serial && self->canSuspend());
  });
}
bool SleepCoordinator::requestSleep(SleepMode mode) {
  if (actionMethod(mode).isEmpty() || !canSuspend()) return false;
  m_manual = true;
  const auto serial = ++m_serial;
  m_deadline.start();
  Q_EMIT availabilityChanged();
  const bool accepted = m_runtime.requestSuspend([this, serial, mode] {
    if (!m_started || !m_manual || serial != m_serial ||
        m_state.state() != LockState::Locked || !m_state.presentationProtected() ||
        !m_transport.requestSleep(mode, [this, serial] {
          return m_started && m_manual && serial == m_serial &&
                 m_state.state() == LockState::Locked && m_state.presentationProtected();
        })) refuseManual();
  });
  if (!accepted && m_manual) refuseManual();
  return accepted;
}
void SleepCoordinator::preparing(bool value) {
  if (!m_started) return;
  m_runtime.prepareForSleep(value);
  ++m_serial;
  if (value) {
    // Retire a manual callback before replacing it with the actual system sleep
    // gate. Suspend already sent to logind remains uncertain until its reply.
    m_runtime.cancelSuspend();
    m_sleepGate = true;
    static_cast<void>(m_runtime.requestSuspend([this] {
      if (m_started && m_sleepGate && m_transport.preparingForSleep() &&
          m_state.presentationProtected()) {
        m_transport.releaseDelayInhibitor();
      }
    }));
  } else {
    m_sleepGate = false;
    m_runtime.cancelSuspend();
    refuseManual();
    m_transport.acquireDelayInhibitor();
  }
  Q_EMIT availabilityChanged();
}
void SleepCoordinator::authorityChanged() {
  if (!m_started) return;
  if (!m_transport.available()) {
    ++m_serial; m_sleepGate = false;
    m_runtime.cancelSuspend(); refuseManual();
  }
  syncLockedHint();
  Q_EMIT availabilityChanged();
}
void SleepCoordinator::syncLockedHint() {
  if (!m_started) return;
  const auto state = m_state.state();
  if (state == LockState::Unlocked)
    static_cast<void>(m_transport.setLockedHint(false));
  else if (state == LockState::Locked && m_state.presentationProtected())
    static_cast<void>(m_transport.setLockedHint(true));
  // Unknown/Locking never forge an unlocked hint or release a delay FD.
  Q_EMIT availabilityChanged();
}
void SleepCoordinator::refuseManual() {
  const auto result = m_transport.suspendDispatched() ? SleepResult::Uncertain : SleepResult::Refused;
  // Retire before observers can start another request; old Can/action replies
  // must never complete a new mode in the same logind generation.
  m_transport.cancelSleep();
  finishManual(result);
}
void SleepCoordinator::finishManual(SleepResult result) {
  if (!m_manual) return;
  m_manual = false;
  m_deadline.stop();
  Q_EMIT suspendFinished(result);
  Q_EMIT availabilityChanged();
}
}
