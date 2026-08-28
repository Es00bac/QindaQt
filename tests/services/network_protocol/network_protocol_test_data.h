// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_types.h>

namespace QindaQt::Network::TestData {

inline constexpr char kCafeSsid[] = "Cafe";

inline KnownNetwork cafeNetwork(
    const SecuritySuite security = SecuritySuite::Wpa2Personal) {
  const QByteArray rawSsid(kCafeSsid);
  const SsidIdentity identity = normalizeSsid(rawSsid);
  return KnownNetwork{knownNetworkId(rawSsid, security), identity.text,
                      identity.hidden, security, true};
}

inline Device wifiDevice() {
  return Device{QStringLiteral("wlan0"), DeviceKind::Wifi,
                DeviceState::Disconnected};
}

inline Device ethernetDevice() {
  return Device{QStringLiteral("enp3s0"), DeviceKind::Ethernet,
                DeviceState::Connected};
}

inline Snapshot validSnapshot() {
  Snapshot snapshot;
  snapshot.protocolVersion = kProtocolVersion;
  snapshot.owner = QStringLiteral(":1.23");
  snapshot.epoch = 41;
  snapshot.revision = 7;
  snapshot.availability = Availability::Ready;
  snapshot.capabilities =
      Capability::Connectivity | Capability::Scan |
      Capability::KnownNetworkControl | Capability::RadioControl |
      Capability::ActiveConnectionControl;
  snapshot.connectivity = ConnectivityKind::Full;
  snapshot.radios = {Radio{RadioKind::Wifi, true, true, true}};
  snapshot.devices = {ethernetDevice(), wifiDevice()};
  snapshot.knownNetworks = {cafeNetwork()};
  snapshot.scanPhase = ScanPhase::Idle;
  return snapshot;
}

inline Snapshot leasedSnapshot(const QString &leaseId = QStringLiteral("lease-1"),
                               const Availability availability =
                                   Availability::Ready) {
  Snapshot snapshot = validSnapshot();
  snapshot.availability = availability;
  snapshot.scanPhase = ScanPhase::Leased;
  snapshot.scanLease = ScanLease{leaseId, snapshot.epoch, snapshot.revision,
                                 120'000};
  return snapshot;
}

inline OperationResult validOperationResult(
    const OperationKind kind = OperationKind::RequestScan,
    const OperationStatus status = OperationStatus::Succeeded) {
  OperationResult result;
  result.kind = kind;
  result.status = status;
  result.initiatingEpoch = 41;
  result.initiatingRevision = 7;
  if (status != OperationStatus::Succeeded) {
    result.reasonCode = QStringLiteral("rejected-reason");
  }
  return result;
}

} // namespace QindaQt::Network::TestData
