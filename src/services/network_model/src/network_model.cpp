// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_model/network_model.h>

#include <qindaqt/services/network_model/network_intent_policy.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_redaction.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <chrono>

namespace QindaQt::Network::Model {
namespace {

qint64 steadyClockMs() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

} // namespace

NetworkModel::NetworkModel() : NetworkModel(MonotonicClock(&steadyClockMs)) {}

NetworkModel::NetworkModel(MonotonicClock clock) : m_clock(std::move(clock)) {}

std::optional<Lineage> NetworkModel::lineage() const noexcept {
  if (!m_current.has_value()) {
    return std::nullopt;
  }
  return Lineage{m_current->owner, m_current->epoch, m_current->revision};
}

NetworkModel::ApplyResult NetworkModel::applySnapshot(const Snapshot &candidate) {
  const ValidationResult validation = validateSnapshot(candidate);
  if (!validation.accepted) {
    return {false, redactDiagnostic(validation.reasonCode)};
  }
  const GateDecision decision = gateSnapshot(m_lineageHighWater, candidate);
  if (!decision.accepted()) {
    return {false, redactDiagnostic(decision.reasonCode)};
  }
  // AGENT-GUARD: Atomic publication — build the complete replacement state
  // first and swap both the snapshot and the lease tracker only after every
  // check passed. An early return here must leave the model untouched.
  std::optional<Snapshot> next = candidate;
  ScanLeaseTracker lease;
  if (candidate.scanPhase == ScanPhase::Idle) {
    lease.release();
  } else if (!lease.adopt(candidate.scanLease, candidate.epoch, m_clock)) {
    return {false, QStringLiteral("scan-lease-deadline-out-of-bounds")};
  }
  const Lineage nextLineage{candidate.owner, candidate.epoch,
                            candidate.revision};
  m_current = std::move(next);
  m_lineageHighWater = nextLineage;
  m_lease = std::move(lease);
  return {true, {}};
}

void NetworkModel::clear() noexcept {
  m_current.reset();
  m_lease.release();
}

IntentVerdict NetworkModel::requestScan(const RequestScanIntent &intent) const {
  return validateRequestScan(m_current, m_lease, m_clock, intent);
}

IntentVerdict NetworkModel::connectKnown(const ConnectIntent &intent) const {
  return validateConnect(m_current, intent);
}

IntentVerdict NetworkModel::disconnectDevice(
    const DisconnectIntent &intent) const {
  return validateDisconnect(m_current, intent);
}

IntentVerdict NetworkModel::setRadio(const SetRadioIntent &intent) const {
  return validateSetRadio(m_current, intent);
}

ModelState NetworkModel::projection(const bool operationInFlight) const {
  ModelState state;
  if (!m_current.has_value()) {
    state.availability = Availability::Unavailable;
    state.reasonCode = QStringLiteral("no-snapshot");
    return state;
  }
  const Snapshot &snapshot = *m_current;
  state.hasSnapshot = true;
  state.owner = snapshot.owner;
  state.epoch = snapshot.epoch;
  state.revision = snapshot.revision;
  state.availability = snapshot.availability;
  state.reasonCode = redactDiagnostic(snapshot.reasonCode);
  state.diagnostic = redactDiagnostic(snapshot.diagnostic);
  state.connectivity = snapshot.connectivity;
  state.radios = snapshot.radios;
  state.devices = snapshot.devices;
  state.accessPoints = snapshot.accessPoints;
  state.knownNetworks = snapshot.knownNetworks;
  state.activeConnections = snapshot.activeConnections;
  state.scanPhase = snapshot.scanPhase;
  state.scanCapable = snapshot.capabilities.testFlag(Capability::Scan);
  state.radioControlCapable =
      snapshot.capabilities.testFlag(Capability::RadioControl);
  const bool leaseExpired =
      snapshot.scanPhase == ScanPhase::Leased && m_lease.expired(m_clock);
  state.scanLeaseExpired = leaseExpired;
  state.scanLeaseRemainingMs = leaseExpired
                                   ? 0
                                   : (snapshot.scanPhase == ScanPhase::Leased
                                          ? m_lease.remainingMs(m_clock)
                                          : 0);
  state.scanBusy =
      snapshot.scanPhase == ScanPhase::Scanning || operationInFlight;
  return state;
}

QString accessPointDisplayName(const AccessPoint &point) {
  if (point.hidden || point.ssid.isEmpty()) {
    return QStringLiteral("Hidden network");
  }
  return point.ssid;
}

} // namespace QindaQt::Network::Model
