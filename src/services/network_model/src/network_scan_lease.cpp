// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_model/network_scan_lease.h>

namespace QindaQt::Network::Model {

ScanLeaseTracker::ScanLeaseTracker() = default;

void ScanLeaseTracker::adopt(const ScanLease &lease, const quint64 epoch) {
  if (lease.grantedEpoch != epoch) {
    return;
  }
  m_lease = lease;
}

bool ScanLeaseTracker::active(const MonotonicClock &clock) const {
  return m_lease.has_value() && !expired(clock);
}

bool ScanLeaseTracker::expired(const MonotonicClock &clock) const {
  if (!m_lease.has_value()) {
    return false;
  }
  const qint64 now = clock ? clock() : 0;
  return now >= m_lease->deadlineEpochMs;
}

qint64 ScanLeaseTracker::remainingMs(const MonotonicClock &clock) const {
  if (!m_lease.has_value() || expired(clock)) {
    return 0;
  }
  const qint64 now = clock ? clock() : 0;
  return m_lease->deadlineEpochMs - now;
}

} // namespace QindaQt::Network::Model
