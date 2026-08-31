// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_manager_adapter/network_manager_facts.h>

#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_service/network_backend.h>

#include <QtCore/QSet>

#include <algorithm>

namespace QindaQt::Network::NetworkManager {
namespace {

template <typename Value, typename Less>
void sorted(QList<Value> &values, Less less) {
  std::sort(values.begin(), values.end(), std::move(less));
}

} // namespace

Service::BackendObservation mapNetworkManagerFacts(const Facts &facts) {
  Service::BackendObservation result;
  if (!facts.available) {
    result.availability = Availability::Unavailable;
    result.reasonCode = QStringLiteral("networkmanager-unavailable");
    return result;
  }

  bool degraded = false;
  result.availability = Availability::Ready;
  result.connectivity = facts.connectivity;

  QSet<RadioKind> radioKinds;
  QList<Radio> radios = facts.radios;
  sorted(radios, [](const Radio &left, const Radio &right) {
    return static_cast<quint32>(left.kind) < static_cast<quint32>(right.kind);
  });
  for (const Radio &radio : std::as_const(radios)) {
    if (result.radios.size() >= kMaxRadios || radioKinds.contains(radio.kind)) {
      degraded = true;
      continue;
    }
    radioKinds.insert(radio.kind);
    result.radios.append(radio);
  }

  QSet<QString> deviceInterfaces;
  QList<Device> devices = facts.devices;
  sorted(devices, [](const Device &left, const Device &right) {
    return left.interfaceName < right.interfaceName;
  });
  for (Device device : std::as_const(devices)) {
    QString normalized;
    if (result.devices.size() >= kMaxDevices ||
        !normalizeInterfaceName(device.interfaceName, &normalized) ||
        deviceInterfaces.contains(normalized)) {
      degraded = true;
      continue;
    }
    device.interfaceName = normalized;
    deviceInterfaces.insert(normalized);
    result.devices.append(std::move(device));
  }

  QSet<QString> knownIds;
  QList<KnownNetworkFact> known = facts.knownNetworks;
  sorted(known,
         [](const KnownNetworkFact &left, const KnownNetworkFact &right) {
           if (left.rawSsid != right.rawSsid) {
             return left.rawSsid < right.rawSsid;
           }
           return static_cast<quint32>(left.security) <
                  static_cast<quint32>(right.security);
         });
  for (const KnownNetworkFact &fact : std::as_const(known)) {
    const SsidIdentity identity = normalizeSsid(fact.rawSsid);
    const QString id = knownNetworkId(fact.rawSsid, fact.security);
    if (result.knownNetworks.size() >= kMaxKnownNetworks || !identity.valid ||
        id.isEmpty()) {
      degraded = true;
      continue;
    }
    if (knownIds.contains(id)) {
      continue;
    }
    knownIds.insert(id);
    result.knownNetworks.append(
        {id, identity.text, identity.hidden, fact.security, fact.autoConnect});
  }

  QSet<QString> accessPointKeys;
  QList<AccessPointFact> points = facts.accessPoints;
  sorted(points, [](const AccessPointFact &left, const AccessPointFact &right) {
    if (left.deviceInterface != right.deviceInterface) {
      return left.deviceInterface < right.deviceInterface;
    }
    return left.bssid < right.bssid;
  });
  for (const AccessPointFact &fact : std::as_const(points)) {
    QString interfaceName;
    QString bssid;
    const SsidIdentity identity = normalizeSsid(fact.rawSsid);
    const auto device =
        std::find_if(result.devices.cbegin(), result.devices.cend(),
                     [&fact](const Device &candidate) {
                       return candidate.interfaceName == fact.deviceInterface &&
                              candidate.kind == DeviceKind::Wifi;
                     });
    const QString key =
        fact.deviceInterface + QLatin1Char('/') + fact.bssid.toLower();
    if (result.accessPoints.size() >= kMaxAccessPoints || !identity.valid ||
        !normalizeInterfaceName(fact.deviceInterface, &interfaceName) ||
        !normalizeBssid(fact.bssid, &bssid) ||
        device == result.devices.cend() || accessPointKeys.contains(key) ||
        fact.signalStrength > kMaximumSignalStrength ||
        (fact.frequencyMHz != 0 &&
         (fact.frequencyMHz < kMinimumWlanFrequencyMHz ||
          fact.frequencyMHz > kMaximumWlanFrequencyMHz))) {
      degraded = true;
      continue;
    }
    accessPointKeys.insert(key);
    result.accessPoints.append({interfaceName, identity.text, identity.hidden,
                                bssid, fact.security, fact.frequencyMHz,
                                fact.signalStrength});
  }

  QSet<QString> activeDevices;
  QList<ActiveConnectionFact> active = facts.activeConnections;
  sorted(active, [](const ActiveConnectionFact &left,
                    const ActiveConnectionFact &right) {
    return left.deviceInterface < right.deviceInterface;
  });
  for (const ActiveConnectionFact &fact : std::as_const(active)) {
    QString interfaceName;
    const QString id = knownNetworkId(fact.rawSsid, fact.security);
    if (result.activeConnections.size() >= kMaxActiveConnections ||
        !normalizeInterfaceName(fact.deviceInterface, &interfaceName) ||
        !deviceInterfaces.contains(interfaceName) || !knownIds.contains(id) ||
        activeDevices.contains(interfaceName)) {
      degraded = true;
      continue;
    }
    activeDevices.insert(interfaceName);
    result.activeConnections.append({interfaceName, id});
  }

  if (facts.scanSupported) {
    result.capabilities |= Capability::Scan;
  }
  if (facts.knownNetworkControlSupported) {
    result.capabilities |= Capability::KnownNetworkControl;
  }
  if (facts.radioControlSupported) {
    result.capabilities |= Capability::RadioControl;
  }
  if (facts.disconnectSupported) {
    result.capabilities |= Capability::ActiveConnectionControl;
  }
  result.capabilities |= Capability::Connectivity;

  result.scanPhase = facts.scanPhase;
  result.scanLeaseRemainingMilliseconds = facts.scanLeaseRemainingMilliseconds;
  if (result.scanPhase != ScanPhase::Idle &&
      (result.scanLeaseRemainingMilliseconds <
           kMinimumScanDeadlineMilliseconds ||
       result.scanLeaseRemainingMilliseconds >
           kMaximumScanDeadlineMilliseconds)) {
    result.scanPhase = ScanPhase::Idle;
    result.scanLeaseRemainingMilliseconds = 0;
    degraded = true;
  }
  if (degraded) {
    result.availability = Availability::Degraded;
    result.reasonCode = QStringLiteral("networkmanager-data-degraded");
    result.diagnostic =
        QStringLiteral("Some NetworkManager public values were rejected");
  }
  return result;
}

} // namespace QindaQt::Network::NetworkManager
