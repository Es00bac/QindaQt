// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Network::NetworkManager {

struct AccessPointFact final {
  QString deviceInterface;
  QByteArray rawSsid;
  QString bssid;
  SecuritySuite security = SecuritySuite::Open;
  quint32 frequencyMHz = 0;
  quint32 signalStrength = 0;
};

struct KnownNetworkFact final {
  QByteArray rawSsid;
  SecuritySuite security = SecuritySuite::Open;
  bool autoConnect = true;
};

struct ActiveConnectionFact final {
  QString deviceInterface;
  QByteArray rawSsid;
  SecuritySuite security = SecuritySuite::Open;
};

// Deliberately cannot represent a password, PSK, certificate, private key,
// NetworkManager object path, hardware address, driver, UID, or PID. The libnm
// port reads only these public facts and never requests a connection's secret
// settings. See ADR-0052.
struct Facts final {
  bool available = false;
  ConnectivityKind connectivity = ConnectivityKind::Unknown;
  QList<Radio> radios;
  QList<Device> devices;
  QList<AccessPointFact> accessPoints;
  QList<KnownNetworkFact> knownNetworks;
  QList<ActiveConnectionFact> activeConnections;
  bool scanSupported = false;
  bool knownNetworkControlSupported = false;
  bool radioControlSupported = false;
  bool disconnectSupported = false;
  ScanPhase scanPhase = ScanPhase::Idle;
  qint64 scanLeaseRemainingMilliseconds = 0;
};

} // namespace QindaQt::Network::NetworkManager

Q_DECLARE_METATYPE(QindaQt::Network::NetworkManager::Facts)
