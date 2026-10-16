// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/platform/recovery_backoff/recovery_backoff.h"
#include <algorithm>
namespace QindaQt::Platform::Recovery {
QtRecoveryTimer::QtRecoveryTimer(QObject *parent) : RecoveryTimer(parent) {
  m_timer.setSingleShot(true);
  // Recovery delays are >= 250 ms; coarse timers let the kernel batch wakeups.
  m_timer.setTimerType(Qt::CoarseTimer);
  connect(&m_timer, &QTimer::timeout, this, &RecoveryTimer::timeout);
}
void QtRecoveryTimer::start(std::chrono::milliseconds delay) {
  m_timer.start(std::max(delay, std::chrono::milliseconds{0}));
}
void QtRecoveryTimer::stop() { m_timer.stop(); }
bool QtRecoveryTimer::isActive() const { return m_timer.isActive(); }

void ManualRecoveryTimer::start(std::chrono::milliseconds delay) {
  m_requested.append(delay);
  m_active = true;
}
void ManualRecoveryTimer::stop() { m_active = false; }
bool ManualRecoveryTimer::isActive() const { return m_active; }
bool ManualRecoveryTimer::fire() {
  if (!m_active)
    return false;
  m_active = false;
  Q_EMIT timeout();
  return true;
}

std::chrono::milliseconds BackoffPolicy::delayFor(int attempt) const noexcept {
  using std::chrono::milliseconds;
  const auto first = std::max(initial, milliseconds{1});
  const auto ceiling = std::max(maximum, first);
  auto delay = first;
  for (int step = 0; step < attempt && delay < ceiling; ++step)
    delay = std::min(delay * 2, ceiling);
  return std::min(delay, ceiling);
}

RecoveryBackoff::RecoveryBackoff(BackoffPolicy policy,
                                 const RecoveryTimerFactory &timers,
                                 QObject *parent)
    : QObject(parent), m_policy(policy),
      m_timer(timers ? timers() : nullptr) {
  if (!m_timer)
    m_timer = std::make_unique<QtRecoveryTimer>();
  connect(m_timer.get(), &RecoveryTimer::timeout, this,
          &RecoveryBackoff::retry);
}
RecoveryBackoff::~RecoveryBackoff() {
  if (m_timer)
    m_timer->disconnect(this);
}
std::chrono::milliseconds RecoveryBackoff::schedule() {
  if (m_timer->isActive())
    return m_armed;
  m_armed = m_policy.delayFor(m_attempts);
  // Saturate the count well before overflow; the delay is already capped.
  if (m_attempts < 64)
    ++m_attempts;
  m_timer->start(m_armed);
  return m_armed;
}
void RecoveryBackoff::reset() {
  m_timer->stop();
  m_attempts = 0;
}
void RecoveryBackoff::cancel() { m_timer->stop(); }
bool RecoveryBackoff::pending() const { return m_timer->isActive(); }
} // namespace QindaQt::Platform::Recovery
