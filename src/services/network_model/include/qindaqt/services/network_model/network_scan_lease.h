// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <functional>
#include <optional>

namespace QindaQt::Network::Model {

// Injected monotonic clock in milliseconds. Production composition provides a
// steady-clock source; tests provide a deterministic counter. The model never
// constructs a timer.
using MonotonicClock = std::function<qint64()>;

// Tracks the scan lease carried by accepted snapshots against an injected
// clock. The tracker never invents a lease: it only adopts, releases, and
// expires what the authoritative snapshot lineage published.
class ScanLeaseTracker final {
public:
  ScanLeaseTracker();

  [[nodiscard]] const std::optional<ScanLease> &lease() const noexcept {
    return m_lease;
  }

  // Adopts a snapshot-published lease. A lease from an epoch other than
  // `epoch` is refused so a stale snapshot can never resurrect an expired
  // lease after an owner change.
  void adopt(const ScanLease &lease, quint64 epoch);
  void release() noexcept { m_lease.reset(); }

  [[nodiscard]] bool active(const MonotonicClock &clock) const;
  [[nodiscard]] bool expired(const MonotonicClock &clock) const;
  [[nodiscard]] qint64 remainingMs(const MonotonicClock &clock) const;

private:
  std::optional<ScanLease> m_lease;
};

} // namespace QindaQt::Network::Model
