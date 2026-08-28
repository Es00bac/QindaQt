// SPDX-License-Identifier: LGPL-3.0-or-later

#include "power_codec_p.h"

namespace QindaQt::Power::CodecPrivate {

DecodeResult readerFailure(const Reader &reader, QString reasonCode) {
  return {.error = reader.error(), .reasonCode = std::move(reasonCode)};
}

void writeHandle(Writer &writer, const Handle &handle) {
  writer.u64(handle.epoch);
  writer.text(handle.opaqueId);
}

bool readHandle(Reader &reader, Handle &handle) {
  return reader.u64(handle.epoch) &&
         reader.text(handle.opaqueId, kMaxOpaqueIdUtf8Bytes);
}

void writeSourceTruth(Writer &writer, const SourceTruth &truth) {
  writer.boolean(truth.acPresent);
  writer.boolean(truth.onBattery);
  writer.boolean(truth.lidPresent);
  writer.boolean(truth.lidClosed);
  writer.boolean(truth.docked);
  writer.boolean(truth.preparingForSleep);
}

bool readSourceTruth(Reader &reader, SourceTruth &truth) {
  return reader.boolean(truth.acPresent) && reader.boolean(truth.onBattery) &&
         reader.boolean(truth.lidPresent) && reader.boolean(truth.lidClosed) &&
         reader.boolean(truth.docked) &&
         reader.boolean(truth.preparingForSleep);
}

void writeSupply(Writer &writer, const PowerSupply &supply) {
  writeHandle(writer, supply.handle);
  writer.u32(static_cast<quint32>(supply.kind));
  writer.text(supply.vendor);
  writer.text(supply.model);
  writer.boolean(supply.present);
  writer.boolean(supply.percentageKnown);
  writer.real(supply.percentage);
  writer.u32(static_cast<quint32>(supply.level));
  writer.u32(static_cast<quint32>(supply.state));
  writer.boolean(supply.energyKnown);
  writer.real(supply.energyWattHours);
  writer.real(supply.energyFullWattHours);
  writer.boolean(supply.rateKnown);
  writer.real(supply.energyRateWatts);
  writer.boolean(supply.timeToEmptyKnown);
  writer.i64(supply.timeToEmptySeconds);
  writer.boolean(supply.timeToFullKnown);
  writer.i64(supply.timeToFullSeconds);
  writer.u32(static_cast<quint32>(supply.warning));
}

bool readSupply(Reader &reader, PowerSupply &supply) {
  quint32 kind = 0;
  quint32 level = 0;
  quint32 state = 0;
  quint32 warning = 0;
  if (!readHandle(reader, supply.handle) || !reader.u32(kind) ||
      !reader.text(supply.vendor, kMaxNameUtf8Bytes) ||
      !reader.text(supply.model, kMaxNameUtf8Bytes) ||
      !reader.boolean(supply.present) ||
      !reader.boolean(supply.percentageKnown) ||
      !reader.real(supply.percentage) || !reader.u32(level) ||
      !reader.u32(state) || !reader.boolean(supply.energyKnown) ||
      !reader.real(supply.energyWattHours) ||
      !reader.real(supply.energyFullWattHours) ||
      !reader.boolean(supply.rateKnown) ||
      !reader.real(supply.energyRateWatts) ||
      !reader.boolean(supply.timeToEmptyKnown) ||
      !reader.i64(supply.timeToEmptySeconds) ||
      !reader.boolean(supply.timeToFullKnown) ||
      !reader.i64(supply.timeToFullSeconds) || !reader.u32(warning)) {
    return false;
  }
  supply.kind = static_cast<SupplyKind>(kind);
  supply.level = static_cast<BatteryLevel>(level);
  supply.state = static_cast<ChargeState>(state);
  supply.warning = static_cast<WarningLevel>(warning);
  return true;
}

void writeComposite(Writer &writer, const CompositeBattery &composite) {
  writer.boolean(composite.present);
  writer.u32(composite.sourceCount);
  writer.boolean(composite.percentageKnown);
  writer.real(composite.percentage);
  writer.u32(static_cast<quint32>(composite.level));
  writer.u32(static_cast<quint32>(composite.state));
  writer.boolean(composite.netRateKnown);
  writer.real(composite.netRateWatts);
  writer.boolean(composite.timeToEmptyKnown);
  writer.i64(composite.timeToEmptySeconds);
  writer.boolean(composite.timeToFullKnown);
  writer.i64(composite.timeToFullSeconds);
  writer.u32(static_cast<quint32>(composite.warning));
}

bool readComposite(Reader &reader, CompositeBattery &composite) {
  quint32 level = 0;
  quint32 state = 0;
  quint32 warning = 0;
  if (!reader.boolean(composite.present) ||
      !reader.u32(composite.sourceCount) ||
      !reader.boolean(composite.percentageKnown) ||
      !reader.real(composite.percentage) || !reader.u32(level) ||
      !reader.u32(state) || !reader.boolean(composite.netRateKnown) ||
      !reader.real(composite.netRateWatts) ||
      !reader.boolean(composite.timeToEmptyKnown) ||
      !reader.i64(composite.timeToEmptySeconds) ||
      !reader.boolean(composite.timeToFullKnown) ||
      !reader.i64(composite.timeToFullSeconds) || !reader.u32(warning)) {
    return false;
  }
  composite.level = static_cast<BatteryLevel>(level);
  composite.state = static_cast<ChargeState>(state);
  composite.warning = static_cast<WarningLevel>(warning);
  return true;
}

void writeProfileState(Writer &writer, const ProfileState &profiles) {
  writer.text(profiles.activeProfileId);
  writeList(writer, profiles.supported,
            [](Writer &target, const Profile &profile) {
              target.text(profile.id);
              target.text(profile.label);
            });
  writeList(writer, profiles.holds,
            [](Writer &target, const ProfileHold &hold) {
              writeHandle(target, hold.handle);
              target.text(hold.profileId);
              target.text(hold.applicationName);
              target.text(hold.reason);
            });
  writer.text(profiles.degradationReason);
}

bool readProfileState(Reader &reader, ProfileState &profiles) {
  return reader.text(profiles.activeProfileId, kMaxProfileIdUtf8Bytes) &&
         readBoundedList(reader, profiles.supported, kMaxProfiles,
                         [](Reader &source, Profile &profile) {
                           return source.text(profile.id,
                                              kMaxProfileIdUtf8Bytes) &&
                                  source.text(profile.label, kMaxNameUtf8Bytes);
                         }) &&
         readBoundedList(
             reader, profiles.holds, kMaxProfileHolds,
             [](Reader &source, ProfileHold &hold) {
               return readHandle(source, hold.handle) &&
                      source.text(hold.profileId, kMaxProfileIdUtf8Bytes) &&
                      source.text(hold.applicationName, kMaxNameUtf8Bytes) &&
                      source.text(hold.reason, kMaxDiagnosticUtf8Bytes);
             }) &&
         reader.text(profiles.degradationReason, kMaxDiagnosticUtf8Bytes);
}

void writeInhibitor(Writer &writer, const Inhibitor &inhibitor) {
  writer.text(inhibitor.what);
  writer.text(inhibitor.who);
  writer.text(inhibitor.why);
  writer.text(inhibitor.mode);
}

bool readInhibitor(Reader &reader, Inhibitor &inhibitor) {
  return reader.text(inhibitor.what, kMaxInhibitorWhatUtf8Bytes) &&
         reader.text(inhibitor.who, kMaxInhibitorWhoUtf8Bytes) &&
         reader.text(inhibitor.why, kMaxInhibitorWhyUtf8Bytes) &&
         reader.text(inhibitor.mode, kMaxInhibitorModeUtf8Bytes);
}

void writeKeyboardBacklight(Writer &writer, const KeyboardBacklight &device) {
  writeHandle(writer, device.handle);
  writer.text(device.name);
  writer.boolean(device.valueKnown);
  writer.u32(device.value);
  writer.u32(device.maximum);
  writer.u32(device.normalized);
  writer.boolean(device.canSet);
}

bool readKeyboardBacklight(Reader &reader, KeyboardBacklight &device) {
  return readHandle(reader, device.handle) &&
         reader.text(device.name, kMaxNameUtf8Bytes) &&
         reader.boolean(device.valueKnown) && reader.u32(device.value) &&
         reader.u32(device.maximum) && reader.u32(device.normalized) &&
         reader.boolean(device.canSet);
}

void writeInternalBacklight(Writer &writer, const InternalBacklight &device) {
  writeHandle(writer, device.handle);
  writer.text(device.deviceName);
  writer.boolean(device.internal);
  writer.u32(static_cast<quint32>(device.kind));
  writer.u32(device.maximum);
  writer.boolean(device.observedKnown);
  writer.u32(device.observed);
  writer.u32(static_cast<quint32>(device.status));
  writer.u32(static_cast<quint32>(device.reason));
  writer.text(device.diagnostic);
}

bool readInternalBacklight(Reader &reader, InternalBacklight &device) {
  quint32 kind = 0;
  quint32 status = 0;
  quint32 reason = 0;
  if (!readHandle(reader, device.handle) ||
      !reader.text(device.deviceName, kMaxNameUtf8Bytes) ||
      !reader.boolean(device.internal) || !reader.u32(kind) ||
      !reader.u32(device.maximum) || !reader.boolean(device.observedKnown) ||
      !reader.u32(device.observed) || !reader.u32(status) ||
      !reader.u32(reason) ||
      !reader.text(device.diagnostic, kMaxDiagnosticUtf8Bytes)) {
    return false;
  }
  device.kind = static_cast<BacklightKind>(kind);
  device.status = static_cast<BacklightStatus>(status);
  device.reason = static_cast<BacklightReason>(reason);
  return true;
}

void writeWaylandBinding(Writer &writer, const WaylandBinding &binding) {
  writer.boolean(binding.available);
  writer.text(binding.socketName);
  writer.u32(binding.protocolVersion);
  writer.u64(binding.bindingEpoch);
}

bool readWaylandBinding(Reader &reader, WaylandBinding &binding) {
  return reader.boolean(binding.available) &&
         reader.text(binding.socketName, kMaxWaylandSocketUtf8Bytes) &&
         reader.u32(binding.protocolVersion) &&
         reader.u64(binding.bindingEpoch);
}

} // namespace QindaQt::Power::CodecPrivate
