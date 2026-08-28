// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/power_protocol/power_dbus.h>

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtDBus/QDBusMetaType>

namespace QindaQt::Power {
namespace {

template <typename T>
void writeArray(QDBusArgument &argument, const QList<T> &values) {
  argument.beginArray(QMetaType::fromType<T>());
  for (const T &value : values) {
    argument << value;
  }
  argument.endArray();
}

template <typename T>
void readBoundedArray(const QDBusArgument &argument, QList<T> &values,
                      const qsizetype maximum, bool &wireValid) {
  QList<T> decoded;
  argument.beginArray();
  while (!argument.atEnd()) {
    T value;
    argument >> value;
    if (decoded.size() < maximum) {
      decoded.push_back(std::move(value));
    } else {
      wireValid = false;
    }
  }
  argument.endArray();
  values = std::move(decoded);
}

} // namespace

void registerDBusTypes() {
  qDBusRegisterMetaType<Handle>();
  qDBusRegisterMetaType<SourceTruth>();
  qDBusRegisterMetaType<PowerSupply>();
  qDBusRegisterMetaType<CompositeBattery>();
  qDBusRegisterMetaType<Profile>();
  qDBusRegisterMetaType<ProfileHold>();
  qDBusRegisterMetaType<Inhibitor>();
  qDBusRegisterMetaType<KeyboardBacklight>();
  qDBusRegisterMetaType<InternalBacklight>();
  qDBusRegisterMetaType<WaylandBinding>();
  qDBusRegisterMetaType<Snapshot>();
  qDBusRegisterMetaType<OperationResult>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Handle &value) {
  argument.beginStructure();
  argument << value.epoch << value.opaqueId;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Handle &value) {
  argument.beginStructure();
  argument >> value.epoch >> value.opaqueId;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const SourceTruth &value) {
  argument.beginStructure();
  argument << value.acPresent << value.onBattery << value.lidPresent
           << value.lidClosed << value.docked << value.preparingForSleep;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                SourceTruth &value) {
  argument.beginStructure();
  argument >> value.acPresent >> value.onBattery >> value.lidPresent >>
      value.lidClosed >> value.docked >> value.preparingForSleep;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PowerSupply &value) {
  argument.beginStructure();
  argument << value.handle << static_cast<quint32>(value.kind) << value.vendor
           << value.model << value.present << value.percentageKnown
           << value.percentage << static_cast<quint32>(value.level)
           << static_cast<quint32>(value.state) << value.energyKnown
           << value.energyWattHours << value.energyFullWattHours
           << value.rateKnown << value.energyRateWatts << value.timeToEmptyKnown
           << value.timeToEmptySeconds << value.timeToFullKnown
           << value.timeToFullSeconds << static_cast<quint32>(value.warning);
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                PowerSupply &value) {
  quint32 kind = 0;
  quint32 level = 0;
  quint32 state = 0;
  quint32 warning = 0;
  argument.beginStructure();
  argument >> value.handle >> kind >> value.vendor >> value.model >>
      value.present >> value.percentageKnown >> value.percentage >> level >>
      state >> value.energyKnown >> value.energyWattHours >>
      value.energyFullWattHours >> value.rateKnown >> value.energyRateWatts >>
      value.timeToEmptyKnown >> value.timeToEmptySeconds >>
      value.timeToFullKnown >> value.timeToFullSeconds >> warning;
  argument.endStructure();
  value.kind = static_cast<SupplyKind>(kind);
  value.level = static_cast<BatteryLevel>(level);
  value.state = static_cast<ChargeState>(state);
  value.warning = static_cast<WarningLevel>(warning);
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const CompositeBattery &value) {
  argument.beginStructure();
  argument << value.present << value.sourceCount << value.percentageKnown
           << value.percentage << static_cast<quint32>(value.level)
           << static_cast<quint32>(value.state) << value.netRateKnown
           << value.netRateWatts << value.timeToEmptyKnown
           << value.timeToEmptySeconds << value.timeToFullKnown
           << value.timeToFullSeconds << static_cast<quint32>(value.warning);
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                CompositeBattery &value) {
  quint32 level = 0;
  quint32 state = 0;
  quint32 warning = 0;
  argument.beginStructure();
  argument >> value.present >> value.sourceCount >> value.percentageKnown >>
      value.percentage >> level >> state >> value.netRateKnown >>
      value.netRateWatts >> value.timeToEmptyKnown >>
      value.timeToEmptySeconds >> value.timeToFullKnown >>
      value.timeToFullSeconds >> warning;
  argument.endStructure();
  value.level = static_cast<BatteryLevel>(level);
  value.state = static_cast<ChargeState>(state);
  value.warning = static_cast<WarningLevel>(warning);
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Profile &value) {
  argument.beginStructure();
  argument << value.id << value.label;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Profile &value) {
  argument.beginStructure();
  argument >> value.id >> value.label;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ProfileHold &value) {
  argument.beginStructure();
  argument << value.handle << value.profileId << value.applicationName
           << value.reason;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                ProfileHold &value) {
  argument.beginStructure();
  argument >> value.handle >> value.profileId >> value.applicationName >>
      value.reason;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Inhibitor &value) {
  argument.beginStructure();
  argument << value.what << value.who << value.why << value.mode;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Inhibitor &value) {
  argument.beginStructure();
  argument >> value.what >> value.who >> value.why >> value.mode;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const KeyboardBacklight &value) {
  argument.beginStructure();
  argument << value.handle << value.name << value.valueKnown << value.value
           << value.maximum << value.normalized << value.canSet;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                KeyboardBacklight &value) {
  argument.beginStructure();
  argument >> value.handle >> value.name >> value.valueKnown >> value.value >>
      value.maximum >> value.normalized >> value.canSet;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const InternalBacklight &value) {
  argument.beginStructure();
  argument << value.handle << value.deviceName << value.internal
           << static_cast<quint32>(value.kind) << value.maximum
           << value.observedKnown << value.observed
           << static_cast<quint32>(value.status)
           << static_cast<quint32>(value.reason) << value.diagnostic;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                InternalBacklight &value) {
  quint32 kind = 0;
  quint32 status = 0;
  quint32 reason = 0;
  argument.beginStructure();
  argument >> value.handle >> value.deviceName >> value.internal >> kind >>
      value.maximum >> value.observedKnown >> value.observed >> status >>
      reason >> value.diagnostic;
  argument.endStructure();
  value.kind = static_cast<BacklightKind>(kind);
  value.status = static_cast<BacklightStatus>(status);
  value.reason = static_cast<BacklightReason>(reason);
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const WaylandBinding &value) {
  argument.beginStructure();
  argument << value.available << value.socketName << value.protocolVersion
           << value.bindingEpoch;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                WaylandBinding &value) {
  argument.beginStructure();
  argument >> value.available >> value.socketName >> value.protocolVersion >>
      value.bindingEpoch;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value) {
  argument.beginStructure();
  argument << value.protocolVersion << value.epoch << value.revision
           << static_cast<quint32>(value.availability)
           << static_cast<quint32>(value.capabilities.toInt())
           << value.reasonCode << value.diagnostic << value.source
           << value.composite;
  writeArray(argument, value.supplies);
  argument.beginStructure();
  argument << value.profiles.activeProfileId;
  writeArray(argument, value.profiles.supported);
  writeArray(argument, value.profiles.holds);
  argument << value.profiles.degradationReason;
  argument.endStructure();
  writeArray(argument, value.inhibitors);
  writeArray(argument, value.keyboardBacklights);
  writeArray(argument, value.internalBacklights);
  argument << value.waylandBinding;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Snapshot &value) {
  quint32 availability = 0;
  quint32 capabilities = 0;
  value.wireValid = true;
  argument.beginStructure();
  argument >> value.protocolVersion >> value.epoch >> value.revision >>
      availability >> capabilities >> value.reasonCode >> value.diagnostic >>
      value.source >> value.composite;
  readBoundedArray(argument, value.supplies, kMaxPowerSupplies,
                   value.wireValid);
  argument.beginStructure();
  argument >> value.profiles.activeProfileId;
  readBoundedArray(argument, value.profiles.supported, kMaxProfiles,
                   value.wireValid);
  readBoundedArray(argument, value.profiles.holds, kMaxProfileHolds,
                   value.wireValid);
  argument >> value.profiles.degradationReason;
  argument.endStructure();
  readBoundedArray(argument, value.inhibitors, kMaxInhibitors, value.wireValid);
  readBoundedArray(argument, value.keyboardBacklights, kMaxKeyboardBacklights,
                   value.wireValid);
  readBoundedArray(argument, value.internalBacklights, kMaxInternalBacklights,
                   value.wireValid);
  argument >> value.waylandBinding;
  argument.endStructure();
  value.availability = static_cast<Availability>(availability);
  value.capabilities = Capabilities::fromInt(capabilities);
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const OperationResult &value) {
  argument.beginStructure();
  argument << static_cast<quint32>(value.kind)
           << static_cast<quint32>(value.status) << value.initiatingEpoch
           << value.initiatingRevision << value.observedEpoch
           << value.observedRevision << value.reasonCode << value.diagnostic;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                OperationResult &value) {
  quint32 kind = 0;
  quint32 status = 0;
  value.wireValid = true;
  argument.beginStructure();
  argument >> kind >> status >> value.initiatingEpoch >>
      value.initiatingRevision >> value.observedEpoch >>
      value.observedRevision >> value.reasonCode >> value.diagnostic;
  argument.endStructure();
  value.kind = static_cast<OperationKind>(kind);
  value.status = static_cast<OperationStatus>(status);
  return argument;
}

} // namespace QindaQt::Power
