// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_types.h>

namespace QindaQt::Power::TestData {

inline Snapshot validSnapshot() {
  Snapshot snapshot;
  snapshot.protocolVersion = kProtocolVersion;
  snapshot.epoch = 41;
  snapshot.revision = 7;
  snapshot.availability = Availability::Ready;
  snapshot.capabilities =
      Capability::Supplies | Capability::Profiles | Capability::ProfileHolds |
      Capability::Inhibitors | Capability::KeyboardBacklight |
      Capability::InternalBacklight | Capability::Lid | Capability::IdleHint;
  snapshot.reasonCode = QStringLiteral("ready");
  snapshot.source = {.acPresent = false,
                     .onBattery = true,
                     .lidPresent = true,
                     .lidClosed = false,
                     .docked = false,
                     .preparingForSleep = false};
  snapshot.composite = {.present = true,
                        .sourceCount = 1,
                        .percentageKnown = true,
                        .percentage = 37.5,
                        .level = BatteryLevel::None,
                        .state = ChargeState::Discharging,
                        .netRateKnown = true,
                        .netRateWatts = -12.5,
                        .timeToEmptyKnown = true,
                        .timeToEmptySeconds = 7'200,
                        .timeToFullKnown = false,
                        .timeToFullSeconds = 0,
                        .warning = WarningLevel::Low};
  snapshot.supplies = {
      {.handle = {.epoch = 41, .opaqueId = QStringLiteral("supply-a")},
       .kind = SupplyKind::Battery,
       .vendor = QStringLiteral("Qinda"),
       .model = QStringLiteral("Cell"),
       .present = true,
       .percentageKnown = true,
       .percentage = 37.5,
       .level = BatteryLevel::None,
       .state = ChargeState::Discharging,
       .energyKnown = true,
       .energyWattHours = 30.0,
       .energyFullWattHours = 80.0,
       .rateKnown = true,
       .energyRateWatts = 12.5,
       .timeToEmptyKnown = true,
       .timeToEmptySeconds = 7'200,
       .timeToFullKnown = false,
       .timeToFullSeconds = 0,
       .warning = WarningLevel::Low}};
  snapshot.profiles.activeProfileId = QStringLiteral("balanced");
  snapshot.profiles.supported = {
      {.id = QStringLiteral("balanced"), .label = QStringLiteral("Balanced")},
      {.id = QStringLiteral("performance"),
       .label = QStringLiteral("Performance")},
  };
  snapshot.profiles.holds = {
      {.handle = {.epoch = 41, .opaqueId = QStringLiteral("hold-a")},
       .profileId = QStringLiteral("performance"),
       .applicationName = QStringLiteral("Renderer"),
       .reason = QStringLiteral("Exporting")},
  };
  snapshot.inhibitors = {{.what = QStringLiteral("sleep"),
                          .who = QStringLiteral("Backup"),
                          .why = QStringLiteral("Writing archive"),
                          .mode = QStringLiteral("delay")}};
  snapshot.keyboardBacklights = {
      {.handle = {.epoch = 41, .opaqueId = QStringLiteral("kbd-a")},
       .name = QStringLiteral("Keyboard"),
       .valueKnown = true,
       .value = 2,
       .maximum = 3,
       .normalized = 6'667,
       .canSet = true},
  };
  snapshot.internalBacklights = {
      {.handle = {.epoch = 41, .opaqueId = QStringLiteral("panel-a")},
       .deviceName = QStringLiteral("intel_backlight"),
       .internal = true,
       .kind = BacklightKind::Firmware,
       .maximum = 100,
       .observedKnown = true,
       .observed = 50,
       .status = BacklightStatus::Ok,
       .reason = BacklightReason::None,
       .diagnostic = {}},
  };
  snapshot.waylandBinding = {.available = true,
                             .socketName = QStringLiteral("qindaqt-wayland-7"),
                             .protocolVersion = 3,
                             .bindingEpoch = 11};
  return snapshot;
}

inline OperationResult validOperationResult() {
  return {.kind = OperationKind::SetKeyboardBrightness,
          .status = OperationStatus::Succeeded,
          .initiatingEpoch = 41,
          .initiatingRevision = 7,
          .observedEpoch = 41,
          .observedRevision = 8,
          .reasonCode = QStringLiteral("ok"),
          .diagnostic = {},
          .wireValid = true};
}

} // namespace QindaQt::Power::TestData
