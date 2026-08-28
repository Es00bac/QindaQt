// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_model/network_intent_policy.h>

#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <algorithm>

namespace QindaQt::Network::Model {
namespace {

IntentVerdict refuse(const OperationKind kind, const QString &reasonCode) {
  return {false, kind, reasonCode};
}

IntentVerdict allow(const OperationKind kind) { return {true, kind, {}}; }

bool isReady(const std::optional<Snapshot> &snapshot) {
  return snapshot.has_value()
         && snapshot->availability == Availability::Ready;
}

const Radio *findRadio(const Snapshot &snapshot, const RadioKind kind) {
  const auto radio =
      std::find_if(snapshot.radios.cbegin(), snapshot.radios.cend(),
                   [kind](const Radio &candidate) { return candidate.kind == kind; });
  return radio == snapshot.radios.cend() ? nullptr : &*radio;
}

bool hasCapability(const Snapshot &snapshot, const Capability capability) {
  return snapshot.capabilities.testFlag(capability);
}

} // namespace

IntentVerdict validateRequestScan(const std::optional<Snapshot> &snapshot,
                                  const ScanLeaseTracker &lease,
                                  const MonotonicClock &clock,
                                  const RequestScanIntent &intent) {
  if (!isReady(snapshot)) {
    return refuse(OperationKind::RequestScan, QStringLiteral("service-not-ready"));
  }
  if (!hasCapability(*snapshot, Capability::Scan)) {
    return refuse(OperationKind::RequestScan, QStringLiteral("scan-unsupported"));
  }
  if (intent.deadlineMilliseconds < kMinimumScanDeadlineMilliseconds
      || intent.deadlineMilliseconds > kMaximumScanDeadlineMilliseconds) {
    return refuse(OperationKind::RequestScan, QStringLiteral("scan-deadline-out-of-bounds"));
  }
  if (snapshot->scanPhase == ScanPhase::Scanning) {
    return refuse(OperationKind::RequestScan, QStringLiteral("scan-busy"));
  }
  if (snapshot->scanPhase == ScanPhase::Leased && lease.active(clock)) {
    // AGENT-NOTE: A live lease pins the current result set; a second scan
    // request would extend radio time without user benefit, so it is Busy.
    return refuse(OperationKind::RequestScan, QStringLiteral("scan-lease-held"));
  }
  return allow(OperationKind::RequestScan);
}

IntentVerdict validateConnect(const std::optional<Snapshot> &snapshot,
                              const ConnectIntent &intent) {
  if (!isReady(snapshot)) {
    return refuse(OperationKind::ConnectKnownNetwork,
                  QStringLiteral("service-not-ready"));
  }
  if (!hasCapability(*snapshot, Capability::KnownNetworkControl)) {
    return refuse(OperationKind::ConnectKnownNetwork,
                  QStringLiteral("known-network-control-unsupported"));
  }
  if (!isValidKnownNetworkId(intent.knownNetworkId)) {
    return refuse(OperationKind::ConnectKnownNetwork,
                  QStringLiteral("known-network-id-invalid"));
  }
  const auto network = std::find_if(
      snapshot->knownNetworks.cbegin(), snapshot->knownNetworks.cend(),
      [&intent](const KnownNetwork &candidate) {
        return candidate.id == intent.knownNetworkId;
      });
  if (network == snapshot->knownNetworks.cend()) {
    return refuse(OperationKind::ConnectKnownNetwork,
                  QStringLiteral("unknown-known-network"));
  }
  const auto active = std::find_if(
      snapshot->activeConnections.cbegin(),
      snapshot->activeConnections.cend(),
      [&intent](const ActiveConnection &connection) {
        return connection.knownNetworkId == intent.knownNetworkId;
      });
  if (active != snapshot->activeConnections.cend()) {
    return refuse(OperationKind::ConnectKnownNetwork,
                  QStringLiteral("network-already-active"));
  }
  return allow(OperationKind::ConnectKnownNetwork);
}

IntentVerdict validateDisconnect(const std::optional<Snapshot> &snapshot,
                                 const DisconnectIntent &intent) {
  if (!isReady(snapshot)) {
    return refuse(OperationKind::DisconnectActive,
                  QStringLiteral("service-not-ready"));
  }
  if (!hasCapability(*snapshot, Capability::ActiveConnectionControl)) {
    return refuse(OperationKind::DisconnectActive,
                  QStringLiteral("active-connection-control-unsupported"));
  }
  QString normalizedInterface;
  if (!normalizeInterfaceName(intent.deviceInterface, &normalizedInterface)
      || normalizedInterface != intent.deviceInterface) {
    return refuse(OperationKind::DisconnectActive,
                  QStringLiteral("device-interface-invalid"));
  }
  const auto device = std::find_if(
      snapshot->devices.cbegin(), snapshot->devices.cend(),
      [&intent](const Device &candidate) {
        return candidate.interfaceName == intent.deviceInterface;
      });
  if (device == snapshot->devices.cend()) {
    return refuse(OperationKind::DisconnectActive, QStringLiteral("unknown-device"));
  }
  const auto active = std::find_if(
      snapshot->activeConnections.cbegin(),
      snapshot->activeConnections.cend(),
      [&intent](const ActiveConnection &connection) {
        return connection.deviceInterface == intent.deviceInterface;
      });
  if (active == snapshot->activeConnections.cend()) {
    return refuse(OperationKind::DisconnectActive, QStringLiteral("device-not-connected"));
  }
  return allow(OperationKind::DisconnectActive);
}

IntentVerdict validateSetRadio(const std::optional<Snapshot> &snapshot,
                               const SetRadioIntent &intent) {
  if (!isReady(snapshot)) {
    return refuse(OperationKind::SetRadio, QStringLiteral("service-not-ready"));
  }
  if (!hasCapability(*snapshot, Capability::RadioControl)) {
    return refuse(OperationKind::SetRadio,
                  QStringLiteral("radio-control-unsupported"));
  }
  if (static_cast<quint32>(intent.kind)
      > static_cast<quint32>(RadioKind::Wwan)) {
    return refuse(OperationKind::SetRadio,
                  QStringLiteral("radio-kind-invalid"));
  }
  const Radio *radio = findRadio(*snapshot, intent.kind);
  if (radio == nullptr || !radio->present) {
    return refuse(OperationKind::SetRadio, QStringLiteral("radio-absent"));
  }
  if (!radio->hardwareEnabled) {
    // A hardware-disabled radio can never be enabled in software; admitting
    // the intent would publish a mutation that must fail later.
    return refuse(OperationKind::SetRadio, QStringLiteral("radio-hardware-disabled"));
  }
  if (radio->softwareEnabled == intent.enable) {
    return refuse(OperationKind::SetRadio, QStringLiteral("radio-already-in-state"));
  }
  return allow(OperationKind::SetRadio);
}

} // namespace QindaQt::Network::Model
