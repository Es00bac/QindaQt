// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_model/network_intent_policy.h>
#include <qindaqt/services/network_model/network_model_state.h>
#include <qindaqt/services/network_model/network_scan_lease.h>
#include <qindaqt/services/network_model/network_snapshot_gate.h>
#include <qindaqt/services/network_protocol/network_intent.h>

#include <optional>

namespace QindaQt::Network::Model {

// Composes validation, the lineage gate, scan-lease reconciliation, intent
// admission, and the atomic consumer projection for Network N0. The class is
// a plain value-domain object: single-threaded like its callers, it owns no
// timers, transport, or platform objects. All state changes are whole-value
// swaps; a rejected candidate leaves every observable field untouched.
class NetworkModel final {
public:
  // A steady-clock default; deterministic tests inject their own clock.
  NetworkModel();
  explicit NetworkModel(MonotonicClock clock);

  struct ApplyResult final {
    bool accepted = false;
    QString reasonCode;

    friend bool operator==(const ApplyResult &, const ApplyResult &) = default;
  };

  // Validates and gates `candidate`, then publishes it atomically together
  // with its scan-lease truth. Validation failures and gate rejections never
  // modify the current state; the returned reason code is already redacted.
  [[nodiscard]] ApplyResult applySnapshot(const Snapshot &candidate);

  // Clears published snapshot and lease truth while retaining the lineage
  // high-water fence for this model lifetime. Owner loss must not make a
  // retired epoch admissible after A -> B -> A. Idempotent.
  void clear() noexcept;

  [[nodiscard]] const std::optional<Snapshot> &snapshot() const noexcept {
    return m_current;
  }
  [[nodiscard]] std::optional<Lineage> lineage() const noexcept;
  [[nodiscard]] const std::optional<Lineage> &lineageHighWater() const noexcept {
    return m_lineageHighWater;
  }
  [[nodiscard]] const ScanLeaseTracker &scanLease() const noexcept {
    return m_lease;
  }
  [[nodiscard]] qint64 now() const { return m_clock ? m_clock() : 0; }

  [[nodiscard]] IntentVerdict requestScan(const RequestScanIntent &intent) const;
  [[nodiscard]] IntentVerdict connectKnown(const ConnectIntent &intent) const;
  [[nodiscard]] IntentVerdict disconnectDevice(const DisconnectIntent &intent) const;
  [[nodiscard]] IntentVerdict setRadio(const SetRadioIntent &intent) const;

  // Whole-state projection. `operationInFlight` marks a client mutation as
  // busy truth; it never fabricates snapshot content.
  [[nodiscard]] ModelState projection(bool operationInFlight = false) const;

private:
  MonotonicClock m_clock;
  std::optional<Snapshot> m_current;
  std::optional<Lineage> m_lineageHighWater;
  ScanLeaseTracker m_lease;
};

} // namespace QindaQt::Network::Model
