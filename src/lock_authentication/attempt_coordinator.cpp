// SPDX-License-Identifier: GPL-3.0-or-later
#include "attempt_coordinator.h"
namespace QindaQt::LockAuthentication {
void AttemptCoordinator::enterLockedSession() {
  ++m_epoch;
  m_locked = true;
  m_pending.reset();
}
void AttemptCoordinator::leaveLockedSession() {
  ++m_epoch;
  m_locked = false;
  m_pending.reset();
}
std::optional<AttemptToken> AttemptCoordinator::begin() {
  if (!m_locked || m_pending) {
    return std::nullopt;
  }
  m_pending = AttemptToken{m_epoch, ++m_request};
  return m_pending;
}
void AttemptCoordinator::cancel() { m_pending.reset(); }
Completion AttemptCoordinator::complete(AttemptToken token, Outcome outcome) {
  if (!m_locked || !m_pending || *m_pending != token) {
    return Completion::Ignored;
  }
  m_pending.reset();
  if (outcome == Outcome::Authenticated) {
    // Consume the entire lock epoch, not just this worker response.
    m_locked = false;
    return Completion::Unlock;
  }
  return Completion::Retry;
}
} // namespace QindaQt::LockAuthentication
