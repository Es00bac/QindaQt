// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_protocol/network_validation.h>

#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtCore/QSet>

#include <algorithm>

namespace QindaQt::Network {
namespace {

ValidationResult reject(const QString &reasonCode) {
  return {false, reasonCode};
}

ValidationResult accept() { return {true, {}}; }

bool inRange(const quint32 value, const quint32 maximum) {
  return value <= maximum;
}

bool isPrintableText(const QString &value) {
  for (const QChar character : value) {
    const char32_t code = character.unicode();
    if (code <= 0x1FU || code == 0x7FU) {
      return false;
    }
  }
  return true;
}

bool isNormalizedSsidText(const QString &ssid) {
  return !ssid.isEmpty() && isPrintableText(ssid)
         && isBoundedText(ssid, kMaxSsidUtf8Bytes);
}

// Known-network ids are exactly the 64 lowercase hex characters produced by
// knownNetworkId(); anything else is an attacker-chosen opaque handle.
bool isDerivedNetworkId(const QString &id) {
  if (id.size() != 64) {
    return false;
  }
  return std::all_of(id.cbegin(), id.cend(), [](const QChar character) {
    const char16_t code = character.unicode();
    return (code >= u'0' && code <= u'9') || (code >= u'a' && code <= u'f');
  });
}

bool hasUniqueRadioKinds(const QList<Radio> &radios) {
  QSet<quint32> seen;
  seen.reserve(radios.size());
  for (const Radio &radio : radios) {
    if (seen.contains(static_cast<quint32>(radio.kind))) {
      return false;
    }
    seen.insert(static_cast<quint32>(radio.kind));
  }
  return true;
}

bool hasUniqueInterfaces(const QList<Device> &devices) {
  QSet<QString> seen;
  seen.reserve(devices.size());
  for (const Device &device : devices) {
    if (seen.contains(device.interfaceName)) {
      return false;
    }
    seen.insert(device.interfaceName);
  }
  return true;
}

bool hasUniqueDeviceBssids(const QList<AccessPoint> &points) {
  QSet<QString> seen;
  seen.reserve(points.size());
  for (const AccessPoint &point : points) {
    const QString key = point.deviceInterface + QLatin1Char(':') + point.bssid;
    if (seen.contains(key)) {
      return false;
    }
    seen.insert(key);
  }
  return true;
}

bool hasUniqueNetworkIds(const QList<KnownNetwork> &networks) {
  QSet<QString> seen;
  seen.reserve(networks.size());
  for (const KnownNetwork &network : networks) {
    if (seen.contains(network.id)) {
      return false;
    }
    seen.insert(network.id);
  }
  return true;
}

bool hasUniqueConnectionDevices(const QList<ActiveConnection> &connections) {
  QSet<QString> seen;
  seen.reserve(connections.size());
  for (const ActiveConnection &connection : connections) {
    if (seen.contains(connection.deviceInterface)) {
      return false;
    }
    seen.insert(connection.deviceInterface);
  }
  return true;
}

} // namespace

bool isBoundedText(const QString &value, const qsizetype maximumUtf8Bytes) {
  return !value.contains(QChar::Null)
         && value.toUtf8().size() <= maximumUtf8Bytes;
}

ValidationResult validateRadio(const Radio &radio) {
  if (!inRange(static_cast<quint32>(radio.kind),
               static_cast<quint32>(RadioKind::Wwan))) {
    return reject(QStringLiteral("radio-kind-out-of-range"));
  }
  return accept();
}

ValidationResult validateDevice(const Device &device) {
  if (!inRange(static_cast<quint32>(device.kind),
               static_cast<quint32>(DeviceKind::Wwan))
      || !inRange(static_cast<quint32>(device.state),
                  static_cast<quint32>(DeviceState::Failed))) {
    return reject(QStringLiteral("device-enum-out-of-range"));
  }
  QString normalized;
  if (!normalizeInterfaceName(device.interfaceName, &normalized)
      || normalized != device.interfaceName) {
    return reject(QStringLiteral("device-interface-not-normalized"));
  }
  return accept();
}

ValidationResult validateAccessPoint(const AccessPoint &point,
                                     const Snapshot &whole) {
  if (!inRange(static_cast<quint32>(point.security),
               static_cast<quint32>(SecuritySuite::Wpa3Enterprise))) {
    return reject(QStringLiteral("access-point-security-out-of-range"));
  }
  QString normalized;
  if (!normalizeInterfaceName(point.deviceInterface, &normalized)
      || normalized != point.deviceInterface) {
    return reject(QStringLiteral("access-point-device-not-normalized"));
  }
  const auto ownerDevice = std::find_if(
      whole.devices.cbegin(), whole.devices.cend(),
      [&point](const Device &device) {
        return device.interfaceName == point.deviceInterface;
      });
  if (ownerDevice == whole.devices.cend()
      || ownerDevice->kind != DeviceKind::Wifi) {
    return reject(QStringLiteral("access-point-device-missing"));
  }
  if (point.bssid.isEmpty()
      || !normalizeBssid(point.bssid, &normalized) || normalized != point.bssid) {
    return reject(QStringLiteral("access-point-bssid-not-normalized"));
  }
  if (point.hidden) {
    if (!point.ssid.isEmpty()) {
      return reject(QStringLiteral("hidden-access-point-with-ssid"));
    }
  } else if (!isNormalizedSsidText(point.ssid)) {
    return reject(QStringLiteral("access-point-ssid-not-normalized"));
  }
  if (point.frequencyMHz != 0
      && (point.frequencyMHz < kMinimumWlanFrequencyMHz
          || point.frequencyMHz > kMaximumWlanFrequencyMHz)) {
    return reject(QStringLiteral("access-point-frequency-out-of-range"));
  }
  if (point.signalStrength > kMaximumSignalStrength) {
    return reject(QStringLiteral("access-point-signal-out-of-range"));
  }
  return accept();
}

ValidationResult validateKnownNetwork(const KnownNetwork &network) {
  if (!inRange(static_cast<quint32>(network.security),
               static_cast<quint32>(SecuritySuite::Wpa3Enterprise))) {
    return reject(QStringLiteral("known-network-security-out-of-range"));
  }
  if (!isDerivedNetworkId(network.id)) {
    return reject(QStringLiteral("known-network-id-not-derived"));
  }
  if (network.hidden) {
    if (!network.ssid.isEmpty()) {
      return reject(QStringLiteral("hidden-known-network-with-ssid"));
    }
  } else if (!isNormalizedSsidText(network.ssid)) {
    return reject(QStringLiteral("known-network-ssid-not-normalized"));
  }
  return accept();
}

ValidationResult validateActiveConnection(const ActiveConnection &connection,
                                          const Snapshot &whole) {
  QString normalized;
  if (!normalizeInterfaceName(connection.deviceInterface, &normalized)
      || normalized != connection.deviceInterface) {
    return reject(QStringLiteral("active-connection-device-not-normalized"));
  }
  const auto device = std::find_if(
      whole.devices.cbegin(), whole.devices.cend(),
      [&connection](const Device &candidate) {
        return candidate.interfaceName == connection.deviceInterface;
      });
  if (device == whole.devices.cend()) {
    return reject(QStringLiteral("active-connection-device-missing"));
  }
  const auto network = std::find_if(
      whole.knownNetworks.cbegin(), whole.knownNetworks.cend(),
      [&connection](const KnownNetwork &candidate) {
        return candidate.id == connection.knownNetworkId;
      });
  if (network == whole.knownNetworks.cend()) {
    return reject(QStringLiteral("active-connection-network-missing"));
  }
  return accept();
}

ValidationResult validateSnapshot(const Snapshot &snapshot) {
  if (snapshot.protocolVersion != kProtocolVersion) {
    return reject(QStringLiteral("snapshot-protocol-version-unsupported"));
  }
  if (snapshot.epoch == 0 || snapshot.revision == 0) {
    return reject(QStringLiteral("snapshot-lineage-invalid"));
  }
  if (!isBoundedText(snapshot.owner, kMaxOwnerUtf8Bytes)
      || (snapshot.availability != Availability::Starting
          && snapshot.owner.isEmpty())) {
    return reject(QStringLiteral("snapshot-owner-invalid"));
  }
  const Capabilities knownCapabilityBits =
      Capability::Connectivity | Capability::Scan |
      Capability::KnownNetworkControl | Capability::RadioControl |
      Capability::ActiveConnectionControl;
  if (!inRange(static_cast<quint32>(snapshot.availability),
               static_cast<quint32>(Availability::Degraded))
      || !inRange(static_cast<quint32>(snapshot.connectivity),
                  static_cast<quint32>(ConnectivityKind::Full))
      || (snapshot.capabilities & ~knownCapabilityBits).toInt() != 0) {
    return reject(QStringLiteral("snapshot-enum-or-capability-out-of-range"));
  }
  if (!isBoundedText(snapshot.reasonCode, kMaxReasonCodeUtf8Bytes)
      || !isBoundedText(snapshot.diagnostic, kMaxDiagnosticUtf8Bytes)) {
    return reject(QStringLiteral("snapshot-text-out-of-bounds"));
  }
  if ((snapshot.availability == Availability::Unavailable
       || snapshot.availability == Availability::Degraded)
      && snapshot.reasonCode.isEmpty()) {
    return reject(QStringLiteral("snapshot-reason-required"));
  }
  if (snapshot.radios.size() > kMaxRadios || !hasUniqueRadioKinds(snapshot.radios)) {
    return reject(QStringLiteral("snapshot-radios-invalid"));
  }
  for (const Radio &radio : snapshot.radios) {
    if (const ValidationResult result = validateRadio(radio); !result.accepted) {
      return result;
    }
  }
  if (snapshot.devices.size() > kMaxDevices
      || !hasUniqueInterfaces(snapshot.devices)) {
    return reject(QStringLiteral("snapshot-devices-invalid"));
  }
  for (const Device &device : snapshot.devices) {
    if (const ValidationResult result = validateDevice(device); !result.accepted) {
      return result;
    }
  }
  if (snapshot.accessPoints.size() > kMaxAccessPoints
      || !hasUniqueDeviceBssids(snapshot.accessPoints)) {
    return reject(QStringLiteral("snapshot-access-points-invalid"));
  }
  for (const AccessPoint &point : snapshot.accessPoints) {
    if (const ValidationResult result = validateAccessPoint(point, snapshot);
        !result.accepted) {
      return result;
    }
  }
  if (snapshot.knownNetworks.size() > kMaxKnownNetworks
      || !hasUniqueNetworkIds(snapshot.knownNetworks)) {
    return reject(QStringLiteral("snapshot-known-networks-invalid"));
  }
  for (const KnownNetwork &network : snapshot.knownNetworks) {
    if (const ValidationResult result = validateKnownNetwork(network);
        !result.accepted) {
      return result;
    }
  }
  if (snapshot.activeConnections.size() > kMaxActiveConnections
      || !hasUniqueConnectionDevices(snapshot.activeConnections)) {
    return reject(QStringLiteral("snapshot-active-connections-invalid"));
  }
  for (const ActiveConnection &connection : snapshot.activeConnections) {
    if (const ValidationResult result =
            validateActiveConnection(connection, snapshot);
        !result.accepted) {
      return result;
    }
  }
  if (!inRange(static_cast<quint32>(snapshot.scanPhase),
               static_cast<quint32>(ScanPhase::Scanning))) {
    return reject(QStringLiteral("snapshot-scan-phase-out-of-range"));
  }
  if (snapshot.scanPhase == ScanPhase::Idle) {
    if (snapshot.scanLease != ScanLease{}) {
      return reject(QStringLiteral("snapshot-idle-scan-with-lease"));
    }
  } else if (!isBoundedText(snapshot.scanLease.leaseId, kMaxLeaseIdUtf8Bytes)
             || snapshot.scanLease.leaseId.isEmpty()
             || snapshot.scanLease.grantedEpoch != snapshot.epoch
             || snapshot.scanLease.grantedRevision > snapshot.revision
             || snapshot.scanLease.deadlineEpochMs == 0) {
    return reject(QStringLiteral("snapshot-scan-lease-invalid"));
  }
  return accept();
}

ValidationResult validateOperationResult(const OperationResult &result) {
  if (!inRange(static_cast<quint32>(result.kind),
               static_cast<quint32>(OperationKind::SetRadio))
      || !inRange(static_cast<quint32>(result.status),
                  static_cast<quint32>(OperationStatus::Busy))) {
    return reject(QStringLiteral("operation-enum-out-of-range"));
  }
  if (result.initiatingEpoch == 0 || result.initiatingRevision == 0) {
    return reject(QStringLiteral("operation-lineage-invalid"));
  }
  if (!isBoundedText(result.reasonCode, kMaxReasonCodeUtf8Bytes)
      || !isBoundedText(result.diagnostic, kMaxDiagnosticUtf8Bytes)) {
    return reject(QStringLiteral("operation-text-out-of-bounds"));
  }
  if (result.status != OperationStatus::Succeeded && result.reasonCode.isEmpty()) {
    return reject(QStringLiteral("operation-reason-required"));
  }
  return accept();
}

} // namespace QindaQt::Network
