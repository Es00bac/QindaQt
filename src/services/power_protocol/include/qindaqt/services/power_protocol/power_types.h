// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Power {

enum class Availability : quint32 {
  Starting = 0,
  Ready = 1,
  Unavailable = 2,
  Degraded = 3,
};

enum class Capability : quint32 {
  None = 0,
  Supplies = 1U << 0U,
  Profiles = 1U << 1U,
  ProfileHolds = 1U << 2U,
  Inhibitors = 1U << 3U,
  KeyboardBacklight = 1U << 4U,
  InternalBacklight = 1U << 5U,
  Lid = 1U << 6U,
  IdleHint = 1U << 7U,
};
Q_DECLARE_FLAGS(Capabilities, Capability)

enum class SupplyKind : quint32 {
  Battery = 0,
  Ups = 1,
};

enum class ChargeState : quint32 {
  Unknown = 0,
  Charging = 1,
  Discharging = 2,
  Empty = 3,
  FullyCharged = 4,
  PendingCharge = 5,
  PendingDischarge = 6,
};

enum class WarningLevel : quint32 {
  Unknown = 0,
  None = 1,
  Discharging = 2,
  Low = 3,
  Critical = 4,
  Action = 5,
};

// Coarse upstream battery truth is retained only when an exact percentage is
// absent. The numeric values are Power1 wire values, not UPower enum ordinals.
enum class BatteryLevel : quint32 {
  Unknown = 0,
  None = 1,
  Low = 2,
  Critical = 3,
  Normal = 4,
  High = 5,
  Full = 6,
};

enum class BacklightKind : quint32 {
  Firmware = 0,
  Platform = 1,
  Raw = 2,
};

enum class BacklightStatus : quint32 {
  Ok = 0,
  Degraded = 1,
  Unavailable = 2,
};

enum class BacklightReason : quint32 {
  None = 0,
  NoBacklight = 1,
  AmbiguousBacklight = 2,
  NoInternalConnector = 3,
  AmbiguousInternalTopology = 4,
  LogindError = 5,
  DeviceDisappeared = 6,
  NonConverged = 7,
  WaylandUnavailable = 8,
};

enum class OperationKind : quint32 {
  SetProfile = 0,
  AcquireProfileHold = 1,
  ReleaseProfileHold = 2,
  SetKeyboardBrightness = 3,
};

enum class OperationStatus : quint32 {
  Succeeded = 0,
  Rejected = 1,
  Unsupported = 2,
  Failed = 3,
  Uncertain = 4,
  Busy = 5,
  AuthenticationRequired = 6,
  Inhibited = 7,
};

struct Handle {
  quint64 epoch = 0;
  QString opaqueId;

  [[nodiscard]] bool isValid() const noexcept {
    return epoch != 0 && !opaqueId.isEmpty();
  }

  friend bool operator==(const Handle &, const Handle &) = default;
};

struct SourceTruth {
  bool acPresent = false;
  bool onBattery = false;
  bool lidPresent = false;
  bool lidClosed = false;
  bool docked = false;
  bool preparingForSleep = false;

  friend bool operator==(const SourceTruth &, const SourceTruth &) = default;
};

struct PowerSupply {
  Handle handle;
  SupplyKind kind = SupplyKind::Battery;
  QString vendor;
  QString model;
  bool present = true;
  bool percentageKnown = false;
  double percentage = 0.0;
  BatteryLevel level = BatteryLevel::Unknown;
  ChargeState state = ChargeState::Unknown;
  bool energyKnown = false;
  double energyWattHours = 0.0;
  double energyFullWattHours = 0.0;
  bool rateKnown = false;
  double energyRateWatts = 0.0;
  bool timeToEmptyKnown = false;
  qint64 timeToEmptySeconds = 0;
  bool timeToFullKnown = false;
  qint64 timeToFullSeconds = 0;
  WarningLevel warning = WarningLevel::Unknown;

  friend bool operator==(const PowerSupply &, const PowerSupply &) = default;
};

struct CompositeBattery {
  bool present = false;
  quint32 sourceCount = 0;
  bool percentageKnown = false;
  double percentage = 0.0;
  BatteryLevel level = BatteryLevel::Unknown;
  ChargeState state = ChargeState::Unknown;
  bool netRateKnown = false;
  // Positive means aggregate charging; negative means aggregate discharge.
  double netRateWatts = 0.0;
  bool timeToEmptyKnown = false;
  qint64 timeToEmptySeconds = 0;
  bool timeToFullKnown = false;
  qint64 timeToFullSeconds = 0;
  WarningLevel warning = WarningLevel::Unknown;

  friend bool operator==(const CompositeBattery &,
                         const CompositeBattery &) = default;
};

struct Profile {
  QString id;
  QString label;

  friend bool operator==(const Profile &, const Profile &) = default;
};

struct ProfileHold {
  Handle handle;
  QString profileId;
  QString applicationName;
  QString reason;

  friend bool operator==(const ProfileHold &, const ProfileHold &) = default;
};

struct ProfileState {
  QString activeProfileId;
  QList<Profile> supported;
  QList<ProfileHold> holds;
  QString degradationReason;

  friend bool operator==(const ProfileState &, const ProfileState &) = default;
};

// UID and PID are intentionally not representable in this public value. This
// makes the privacy rule structural rather than dependent on redaction at each
// call site.
struct Inhibitor {
  QString what;
  QString who;
  QString why;
  QString mode;

  friend bool operator==(const Inhibitor &, const Inhibitor &) = default;
};

struct KeyboardBacklight {
  Handle handle;
  QString name;
  bool valueKnown = false;
  quint32 value = 0;
  quint32 maximum = 0;
  quint32 normalized = 0;
  bool canSet = false;

  friend bool operator==(const KeyboardBacklight &,
                         const KeyboardBacklight &) = default;
};

struct InternalBacklight {
  Handle handle;
  QString deviceName;
  bool internal = true;
  BacklightKind kind = BacklightKind::Firmware;
  quint32 maximum = 0;
  bool observedKnown = false;
  quint32 observed = 0;
  BacklightStatus status = BacklightStatus::Unavailable;
  BacklightReason reason = BacklightReason::NoBacklight;
  QString diagnostic;

  friend bool operator==(const InternalBacklight &,
                         const InternalBacklight &) = default;
};

struct WaylandBinding {
  bool available = false;
  QString socketName;
  quint32 protocolVersion = 0;
  quint64 bindingEpoch = 0;

  friend bool operator==(const WaylandBinding &,
                         const WaylandBinding &) = default;
};

struct Snapshot {
  quint32 protocolVersion = 1;
  quint64 epoch = 0;
  quint64 revision = 0;
  Availability availability = Availability::Starting;
  Capabilities capabilities;
  QString reasonCode;
  QString diagnostic;
  SourceTruth source;
  CompositeBattery composite;
  QList<PowerSupply> supplies;
  ProfileState profiles;
  QList<Inhibitor> inhibitors;
  QList<KeyboardBacklight> keyboardBacklights;
  QList<InternalBacklight> internalBacklights;
  WaylandBinding waylandBinding;
  bool wireValid = true;

  friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationResult {
  OperationKind kind = OperationKind::SetProfile;
  OperationStatus status = OperationStatus::Failed;
  quint64 initiatingEpoch = 0;
  quint64 initiatingRevision = 0;
  quint64 observedEpoch = 0;
  quint64 observedRevision = 0;
  QString reasonCode;
  QString diagnostic;
  bool wireValid = true;

  friend bool operator==(const OperationResult &,
                         const OperationResult &) = default;
};

} // namespace QindaQt::Power

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Power::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Power::Availability)
Q_DECLARE_METATYPE(QindaQt::Power::Capabilities)
Q_DECLARE_METATYPE(QindaQt::Power::SupplyKind)
Q_DECLARE_METATYPE(QindaQt::Power::ChargeState)
Q_DECLARE_METATYPE(QindaQt::Power::WarningLevel)
Q_DECLARE_METATYPE(QindaQt::Power::BatteryLevel)
Q_DECLARE_METATYPE(QindaQt::Power::BacklightKind)
Q_DECLARE_METATYPE(QindaQt::Power::BacklightStatus)
Q_DECLARE_METATYPE(QindaQt::Power::BacklightReason)
Q_DECLARE_METATYPE(QindaQt::Power::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Power::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Power::Handle)
Q_DECLARE_METATYPE(QindaQt::Power::SourceTruth)
Q_DECLARE_METATYPE(QindaQt::Power::PowerSupply)
Q_DECLARE_METATYPE(QindaQt::Power::CompositeBattery)
Q_DECLARE_METATYPE(QindaQt::Power::Profile)
Q_DECLARE_METATYPE(QindaQt::Power::ProfileHold)
Q_DECLARE_METATYPE(QindaQt::Power::ProfileState)
Q_DECLARE_METATYPE(QindaQt::Power::Inhibitor)
Q_DECLARE_METATYPE(QindaQt::Power::KeyboardBacklight)
Q_DECLARE_METATYPE(QindaQt::Power::InternalBacklight)
Q_DECLARE_METATYPE(QindaQt::Power::WaylandBinding)
Q_DECLARE_METATYPE(QindaQt::Power::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Power::OperationResult)
