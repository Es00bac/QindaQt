// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_client/power_client.h>

namespace QindaQt::Apps::SettingsPower::TestSupport {

class FakePowerTransport final : public Power::PowerTransport {
public:
  using PowerTransport::PowerTransport;
  struct Submission {
    QString owner;
    quint64 requestId = 0;
    Power::PowerClientRequest request;
  };

  void start() override { ++startCount; }
  void stop() override { ++stopCount; }
  void fetchSnapshot(const QString &owner, quint64 requestId) override {
    fetches.append({owner, requestId});
  }
  void submitOperation(const QString &owner, quint64 requestId,
                       const Power::PowerClientRequest &request) override {
    submissions.append({owner, requestId, request});
  }
  void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
  void finishSnapshot(const QString &owner, quint64 requestId,
                      const Power::Snapshot &snapshot) {
    Q_EMIT snapshotReply(owner, requestId, true, snapshot, {});
  }
  void finishOperation(const Submission &submission,
                       const Power::OperationResult &result) {
    Q_EMIT operationReply(submission.owner, submission.requestId, true,
                          result, {});
  }

  QList<QPair<QString, quint64>> fetches;
  QList<Submission> submissions;
  int startCount = 0;
  int stopCount = 0;
};

inline Power::Snapshot readySnapshot(quint64 epoch = 41,
                                     quint64 revision = 7) {
  Power::Snapshot snapshot;
  snapshot.epoch = epoch;
  snapshot.revision = revision;
  snapshot.availability = Power::Availability::Ready;
  snapshot.reasonCode = QStringLiteral("ready");
  snapshot.capabilities = Power::Capability::Supplies
      | Power::Capability::Profiles | Power::Capability::ProfileHolds
      | Power::Capability::KeyboardBacklight
      | Power::Capability::InternalBacklight;
  snapshot.source.acPresent = true;
  snapshot.supplies = {{.handle = {epoch, QStringLiteral("battery-main")},
                        .kind = Power::SupplyKind::Battery,
                        .vendor = QStringLiteral("QindaQt"),
                        .model = QStringLiteral("Main battery"),
                        .present = true,
                        .percentageKnown = true,
                        .percentage = 37.0,
                        .level = Power::BatteryLevel::None,
                        .state = Power::ChargeState::Discharging,
                        .energyKnown = true,
                        .energyWattHours = 18.5,
                        .energyFullWattHours = 50.0,
                        .rateKnown = true,
                        .energyRateWatts = 10.0,
                        .timeToEmptyKnown = true,
                        .timeToEmptySeconds = 6'600,
                        .timeToFullKnown = false,
                        .timeToFullSeconds = 0,
                        .warning = Power::WarningLevel::Low}};
  snapshot.composite = {.present = true,
                        .sourceCount = 1,
                        .percentageKnown = true,
                        .percentage = 37.0,
                        .level = Power::BatteryLevel::None,
                        .state = Power::ChargeState::Discharging,
                        .netRateKnown = true,
                        .netRateWatts = -10.0,
                        .timeToEmptyKnown = true,
                        .timeToEmptySeconds = 6'600,
                        .warning = Power::WarningLevel::Low};
  snapshot.profiles.activeProfileId = QStringLiteral("balanced");
  snapshot.profiles.supported = {
      {.id = QStringLiteral("power-saver"), .label = QStringLiteral("Power Saver")},
      {.id = QStringLiteral("balanced"), .label = QStringLiteral("Balanced")},
      {.id = QStringLiteral("performance"), .label = QStringLiteral("Performance")}};
  snapshot.profiles.holds = {
      {.handle = {epoch, QStringLiteral("hold-video")},
       .profileId = QStringLiteral("performance"),
       .applicationName = QStringLiteral("Video editor"),
       .reason = QStringLiteral("Rendering preview")}};
  snapshot.keyboardBacklights = {
      {.handle = {epoch, QStringLiteral("keyboard-main")},
       .name = QStringLiteral("Built-in keyboard"),
       .valueKnown = true, .value = 5, .maximum = 10,
       .normalized = 5'000, .canSet = true}};
  snapshot.internalBacklights = {
      {.handle = {epoch, QStringLiteral("internal-panel")},
       .deviceName = QStringLiteral("Laptop display"),
       .internal = true, .kind = Power::BacklightKind::Firmware,
       .maximum = 937, .observedKnown = true, .observed = 421,
       .status = Power::BacklightStatus::Ok,
       .reason = Power::BacklightReason::None,
       .diagnostic = {}}};
  return snapshot;
}

inline void publish(Power::PowerClient &client, FakePowerTransport &transport,
                    const Power::Snapshot &snapshot = readySnapshot(),
                    const QString &owner = QStringLiteral(":1.80")) {
  client.start();
  transport.announceOwner(owner);
  transport.finishSnapshot(owner, transport.fetches.constLast().second,
                           snapshot);
}

inline Power::OperationResult success(const FakePowerTransport::Submission &item,
                                      quint64 observedRevision) {
  return {.kind = item.request.kind,
          .status = Power::OperationStatus::Succeeded,
          .initiatingEpoch = 41,
          .initiatingRevision = 7,
          .observedEpoch = 41,
          .observedRevision = observedRevision,
          .reasonCode = QStringLiteral("applied"),
          .diagnostic = {},
          .wireValid = true};
}

} // namespace QindaQt::Apps::SettingsPower::TestSupport
