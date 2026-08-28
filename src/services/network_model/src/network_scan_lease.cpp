// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_model/network_scan_lease.h>

#include <qindaqt/services/network_protocol/network_limits.h>

#include <limits>

namespace QindaQt::Network::Model {

ScanLeaseTracker::ScanLeaseTracker() = default;

bool ScanLeaseTracker::adopt(const ScanLease &lease, const quint64 epoch,
                             const MonotonicClock &clock) {
  if (lease.grantedEpoch != epoch || !clock
      || lease.durationMilliseconds < kMinimumScanDeadlineMilliseconds
      || lease.durationMilliseconds > kMaximumScanDeadlineMilliseconds) {
    return false;
  }
  const qint64 now = clock();
  if (now < 0
      || now > std::numeric_limits<qint64>::max()
                   - lease.durationMilliseconds) {
    return false;
  }
  m_lease = lease;
  m_deadlineMs = now + lease.durationMilliseconds;
  return true;
}

bool ScanLeaseTracker::active(const MonotonicClock &clock) const {
  return m_lease.has_value() && !expired(clock);
}

bool ScanLeaseTracker::expired(const MonotonicClock &clock) const {
  if (!m_lease.has_value()) {
    return false;
  }
  const qint64 now = clock ? clock() : 0;
  return now >= m_deadlineMs;
}

qint64 ScanLeaseTracker::remainingMs(const MonotonicClock &clock) const {
  if (!m_lease.has_value() || expired(clock)) {
    return 0;
  }
  const qint64 now = clock ? clock() : 0;
  return m_deadlineMs - now;
}

} // namespace QindaQt::Network::Model
