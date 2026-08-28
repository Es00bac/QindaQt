// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Network {

enum class Availability : quint32 {
  Starting = 0,
  Ready = 1,
  Unavailable = 2,
  Degraded = 3,
};

enum class Capability : quint32 {
  None = 0,
  Connectivity = 1U << 0U,
  Scan = 1U << 1U,
  KnownNetworkControl = 1U << 2U,
  RadioControl = 1U << 3U,
  ActiveConnectionControl = 1U << 4U,
};
Q_DECLARE_FLAGS(Capabilities, Capability)

enum class ConnectivityKind : quint32 {
  Unknown = 0,
  Offline = 1,
  Portal = 2,
  Limited = 3,
  Full = 4,
};

enum class RadioKind : quint32 {
  Wifi = 0,
  Wwan = 1,
};

enum class DeviceKind : quint32 {
  Ethernet = 0,
  Wifi = 1,
  Wwan = 2,
};

enum class DeviceState : quint32 {
  Unknown = 0,
  Unavailable = 1,
  Disconnected = 2,
  Connecting = 3,
  Connected = 4,
  Disconnecting = 5,
  Failed = 6,
};

enum class SecuritySuite : quint32 {
  Open = 0,
  Wep = 1,
  Wpa2Personal = 2,
  Wpa2Enterprise = 3,
  Wpa3Personal = 4,
  Wpa3Enterprise = 5,
};

enum class ScanPhase : quint32 {
  Idle = 0,
  Leased = 1,
  Scanning = 2,
};

enum class OperationKind : quint32 {
  RequestScan = 0,
  ConnectKnownNetwork = 1,
  DisconnectActive = 2,
  SetRadio = 3,
};

enum class OperationStatus : quint32 {
  Succeeded = 0,
  Rejected = 1,
  Unsupported = 2,
  Failed = 3,
  Uncertain = 4,
  Busy = 5,
};

// Radio hardware/software enable truth. N0 models enable intent and observed
// state only; no N0 code path may mutate a radio.
struct Radio {
  RadioKind kind = RadioKind::Wifi;
  bool present = false;
  bool hardwareEnabled = true;
  bool softwareEnabled = true;

  friend bool operator==(const Radio &, const Radio &) = default;
};

// Device identity is the normalized interface name. There is deliberately no
// persistent system identifier, MAC address, or driver detail in the public
// value: those are private adapter facts that must not reach the shell.
struct Device {
  QString interfaceName;
  DeviceKind kind = DeviceKind::Ethernet;
  DeviceState state = DeviceState::Unknown;

  friend bool operator==(const Device &, const Device &) = default;
};

// AGENT-GUARD: AccessPoint and KnownNetwork structurally cannot carry a
// passphrase, PSK, certificate, or any other secret. Network identity is the
// public SSID text plus a derived correlation pseudonym; adding a secret-bearing
// field here breaks the N0 security contract and the secret redaction tests.
struct AccessPoint {
  QString deviceInterface;
  QString ssid;
  bool hidden = false;
  QString bssid;
  SecuritySuite security = SecuritySuite::Open;
  quint32 frequencyMHz = 0;
  quint32 signalStrength = 0;

  friend bool operator==(const AccessPoint &, const AccessPoint &) = default;
};

struct KnownNetwork {
  QString id;
  QString ssid;
  bool hidden = false;
  SecuritySuite security = SecuritySuite::Open;
  bool autoConnect = true;

  friend bool operator==(const KnownNetwork &, const KnownNetwork &) = default;
};

struct ActiveConnection {
  QString deviceInterface;
  QString knownNetworkId;

  friend bool operator==(const ActiveConnection &,
                         const ActiveConnection &) = default;
};

// A scan lease is the bounded right to keep a scan result set current. Its
// grant epoch binds it to the admitted owner lineage, so an owner change
// invalidates it without trusting any later cleanup message.
struct ScanLease {
  QString leaseId;
  quint64 grantedEpoch = 0;
  quint64 grantedRevision = 0;
  // Remaining lifetime at snapshot publication. A cross-process monotonic
  // epoch is not portable; the consumer converts this bounded duration to its
  // injected local monotonic deadline only after atomic snapshot admission.
  qint64 durationMilliseconds = 0;

  friend bool operator==(const ScanLease &, const ScanLease &) = default;
};

struct Snapshot {
  quint32 protocolVersion = 1;
  QString owner;
  quint64 epoch = 0;
  quint64 revision = 0;
  Availability availability = Availability::Starting;
  Capabilities capabilities;
  ConnectivityKind connectivity = ConnectivityKind::Unknown;
  QString reasonCode;
  QString diagnostic;
  QList<Radio> radios;
  QList<Device> devices;
  QList<AccessPoint> accessPoints;
  QList<KnownNetwork> knownNetworks;
  QList<ActiveConnection> activeConnections;
  ScanPhase scanPhase = ScanPhase::Idle;
  ScanLease scanLease;
  bool wireValid = true;

  friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationResult {
  OperationKind kind = OperationKind::RequestScan;
  OperationStatus status = OperationStatus::Failed;
  quint64 initiatingEpoch = 0;
  quint64 initiatingRevision = 0;
  QString reasonCode;
  QString diagnostic;
  bool wireValid = true;

  friend bool operator==(const OperationResult &,
                         const OperationResult &) = default;
};

} // namespace QindaQt::Network

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Network::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Network::Availability)
Q_DECLARE_METATYPE(QindaQt::Network::Capability)
Q_DECLARE_METATYPE(QindaQt::Network::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Network::ConnectivityKind)
Q_DECLARE_METATYPE(QindaQt::Network::RadioKind)
Q_DECLARE_METATYPE(QindaQt::Network::DeviceKind)
Q_DECLARE_METATYPE(QindaQt::Network::DeviceState)
Q_DECLARE_METATYPE(QindaQt::Network::SecuritySuite)
Q_DECLARE_METATYPE(QindaQt::Network::ScanPhase)
Q_DECLARE_METATYPE(QindaQt::Network::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Network::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Network::Radio)
Q_DECLARE_METATYPE(QindaQt::Network::Device)
Q_DECLARE_METATYPE(QindaQt::Network::AccessPoint)
Q_DECLARE_METATYPE(QindaQt::Network::KnownNetwork)
Q_DECLARE_METATYPE(QindaQt::Network::ActiveConnection)
Q_DECLARE_METATYPE(QindaQt::Network::ScanLease)
Q_DECLARE_METATYPE(QindaQt::Network::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Network::OperationResult)
