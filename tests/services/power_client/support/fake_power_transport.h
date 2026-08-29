// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_client/power_transport.h>

#include <functional>

namespace QindaQt::Tests
{

class FakePowerTransport final : public Power::PowerTransport
{
public:
    using PowerTransport::PowerTransport;

    struct Fetch {
        QString owner;
        quint64 requestId = 0;
    };
    struct Operation {
        QString owner;
        quint64 requestId = 0;
        Power::PowerClientRequest request;
    };

    void start() override { ++startCalls; }
    void stop() override { ++stopCalls; }
    void fetchSnapshot(const QString &owner, const quint64 requestId) override
    {
        fetches.push_back({owner, requestId});
    }
    void submitOperation(const QString &owner, const quint64 requestId,
                         const Power::PowerClientRequest &request) override
    {
        operations.push_back({owner, requestId, request});
        if (operationSubmitted) {
            operationSubmitted(operations.constLast());
        }
    }

    void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
    void invalidate(const QString &owner, const quint64 epoch, const quint64 revision)
    {
        Q_EMIT invalidated(owner, epoch, revision);
    }
    void reply(const Fetch &fetch, const Power::Snapshot &snapshot)
    {
        Q_EMIT snapshotReply(fetch.owner, fetch.requestId, true, snapshot, {});
    }
    void fail(const Fetch &fetch, const QString &reasonCode)
    {
        Q_EMIT snapshotReply(fetch.owner, fetch.requestId, false, {}, reasonCode);
    }
    void finish(const Operation &operation, const Power::OperationResult &result)
    {
        Q_EMIT operationReply(operation.owner, operation.requestId, true, result, {});
    }
    void fail(const Operation &operation, const QString &reasonCode)
    {
        Q_EMIT operationReply(operation.owner, operation.requestId, false, {}, reasonCode);
    }

    QList<Fetch> fetches;
    QList<Operation> operations;
    std::function<void(const Operation &)> operationSubmitted;
    int startCalls = 0;
    int stopCalls = 0;
};

inline Power::Snapshot powerClientSnapshot(const quint64 epoch = 11,
                                           const quint64 revision = 2)
{
    Power::Snapshot snapshot;
    snapshot.epoch = epoch;
    snapshot.revision = revision;
    snapshot.availability = Power::Availability::Ready;
    snapshot.reasonCode = QStringLiteral("ready");
    snapshot.capabilities = Power::Capability::Supplies
        | Power::Capability::KeyboardBacklight | Power::Capability::Profiles
        | Power::Capability::ProfileHolds | Power::Capability::Inhibitors
        | Power::Capability::Lid;
    snapshot.source = {.acPresent = false,
                       .onBattery = true,
                       .lidPresent = true,
                       .lidClosed = false,
                       .docked = false,
                       .preparingForSleep = false};
    snapshot.supplies = {{.handle = {.epoch = epoch, .opaqueId = QStringLiteral("battery-bat0")},
                          .kind = Power::SupplyKind::Battery,
                          .vendor = QStringLiteral("QindaQt"),
                          .model = QStringLiteral("Fixture cell"),
                          .present = true,
                          .percentageKnown = true,
                          .percentage = 55.5,
                          .level = Power::BatteryLevel::None,
                          .state = Power::ChargeState::Discharging,
                          .energyKnown = true,
                          .energyWattHours = 27.5,
                          .energyFullWattHours = 50.0,
                          .rateKnown = true,
                          .energyRateWatts = 9.5,
                          .timeToEmptyKnown = true,
                          .timeToEmptySeconds = 10'432,
                          .timeToFullKnown = false,
                          .timeToFullSeconds = 0,
                          .warning = Power::WarningLevel::Discharging}};
    snapshot.composite = {.present = true,
                          .sourceCount = 1,
                          .percentageKnown = true,
                          .percentage = 55.5,
                          .level = Power::BatteryLevel::None,
                          .state = Power::ChargeState::Discharging,
                          .netRateKnown = true,
                          .netRateWatts = 9.5,
                          .timeToEmptyKnown = true,
                          .timeToEmptySeconds = 10'432,
                          .timeToFullKnown = false,
                          .timeToFullSeconds = 0,
                          .warning = Power::WarningLevel::Discharging};
    snapshot.profiles.supported = {
        {.id = QStringLiteral("power-saver"), .label = QStringLiteral("Power Saver")},
        {.id = QStringLiteral("balanced"), .label = QStringLiteral("Balanced")}};
    snapshot.profiles.activeProfileId = QStringLiteral("balanced");
    snapshot.profiles.holds = {
        {.handle = {.epoch = epoch, .opaqueId = QStringLiteral("hold-1")},
         .profileId = QStringLiteral("balanced"),
         .applicationName = QStringLiteral("QindaQt"),
         .reason = QStringLiteral("fixture hold")}};
    snapshot.inhibitors = {{.what = QStringLiteral("sleep"),
                            .who = QStringLiteral("QindaQt"),
                            .why = QStringLiteral("fixture inhibitor"),
                            .mode = QStringLiteral("delay")}};
    snapshot.keyboardBacklights = {
        {.handle = {.epoch = epoch, .opaqueId = QStringLiteral("keyboard-kbd0")},
         .name = QStringLiteral("Fixture keyboard backlight"),
         .valueKnown = true,
         .value = 128,
         .maximum = 255,
         .normalized = 5'019,
         .canSet = true}};
    return snapshot;
}

inline Power::Handle clientKeyboardHandle(const Power::Snapshot &snapshot)
{
    return snapshot.keyboardBacklights.first().handle;
}

inline Power::Handle clientHoldHandle(const Power::Snapshot &snapshot)
{
    return snapshot.profiles.holds.first().handle;
}

inline Power::OperationResult powerClientResult(const FakePowerTransport::Operation &op,
                                                const Power::OperationStatus status,
                                                const QString &reasonCode)
{
    return {.kind = op.request.kind,
            .status = status,
            .initiatingEpoch = 11,
            .initiatingRevision = 2,
            .observedEpoch = 11,
            .observedRevision = 2,
            .reasonCode = reasonCode,
            .diagnostic = {},
            .wireValid = true};
}

} // namespace QindaQt::Tests
