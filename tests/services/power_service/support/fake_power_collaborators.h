// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

namespace QindaQt::Tests
{

class FakeBatteryCollaborator final : public Power::BatteryCollaborator
{
public:
    using BatteryCollaborator::BatteryCollaborator;

    struct KeyboardOperation {
        quint64 operationId = 0;
        Power::Handle device;
        quint32 value = 0;
    };

    quint64 start() override
    {
        ++startCalls;
        ++generation;
        if (generation == 0) {
            ++generation;
        }
        return generation;
    }
    void stop() override { ++stopCalls; }
    void submitSetKeyboardBrightness(const quint64 operationId,
                                     const Power::Handle &device,
                                     const quint32 value) override
    {
        keyboardOperations.push_back({operationId, device, value});
    }

    void publish(const Power::BatteryFacts &facts) { publishForGeneration(generation, facts); }
    void publishForGeneration(const quint64 runGeneration,
                              const Power::BatteryFacts &facts)
    {
        Q_EMIT factsChanged(runGeneration, facts);
    }
    void publishUnavailable(const QString &reasonCode = QStringLiteral("upstream-unavailable"))
    {
        Q_EMIT statusUnavailable(generation, reasonCode);
    }
    void publishUnavailableForGeneration(const quint64 runGeneration,
                                         const QString &reasonCode)
    {
        Q_EMIT statusUnavailable(runGeneration, reasonCode);
    }
    void replaceAuthority() { Q_EMIT authorityReplaced(generation); }
    void replaceAuthorityForGeneration(const quint64 runGeneration)
    {
        Q_EMIT authorityReplaced(runGeneration);
    }
    void finish(const quint64 operationId, const Power::CollaboratorOutcome &outcome)
    {
        Q_EMIT operationFinished(generation, operationId, outcome);
    }
    void finishForGeneration(const quint64 runGeneration, const quint64 operationId,
                             const Power::CollaboratorOutcome &outcome)
    {
        Q_EMIT operationFinished(runGeneration, operationId, outcome);
    }

    QList<KeyboardOperation> keyboardOperations;
    int startCalls = 0;
    int stopCalls = 0;
    quint64 generation = 0;
};

class FakeProfileCollaborator final : public Power::ProfileCollaborator
{
public:
    using ProfileCollaborator::ProfileCollaborator;

    struct ProfileOperation {
        quint64 operationId = 0;
        Power::OperationKind kind = Power::OperationKind::SetProfile;
        QString profileId;
        QString applicationName;
        QString reason;
        Power::Handle hold;
    };

    quint64 start() override
    {
        ++startCalls;
        ++generation;
        if (generation == 0) {
            ++generation;
        }
        return generation;
    }
    void stop() override { ++stopCalls; }
    void submitSetProfile(const quint64 operationId, const QString &profileId) override
    {
        profileOperations.push_back({operationId, Power::OperationKind::SetProfile,
                                     profileId, {}, {}, {}});
    }
    void submitAcquireProfileHold(const quint64 operationId, const QString &profileId,
                                  const QString &applicationName,
                                  const QString &reason) override
    {
        profileOperations.push_back({operationId, Power::OperationKind::AcquireProfileHold,
                                     profileId, applicationName, reason, {}});
    }
    void submitReleaseProfileHold(const quint64 operationId,
                                  const Power::Handle &hold) override
    {
        ProfileOperation operation;
        operation.operationId = operationId;
        operation.kind = Power::OperationKind::ReleaseProfileHold;
        operation.hold = hold;
        profileOperations.push_back(operation);
    }

    void publish(const Power::ProfileFacts &facts) { publishForGeneration(generation, facts); }
    void publishForGeneration(const quint64 runGeneration,
                              const Power::ProfileFacts &facts)
    {
        Q_EMIT factsChanged(runGeneration, facts);
    }
    void publishUnavailable(const QString &reasonCode = QStringLiteral("upstream-unavailable"))
    {
        Q_EMIT statusUnavailable(generation, reasonCode);
    }
    void publishUnavailableForGeneration(const quint64 runGeneration,
                                         const QString &reasonCode)
    {
        Q_EMIT statusUnavailable(runGeneration, reasonCode);
    }
    void replaceAuthority() { Q_EMIT authorityReplaced(generation); }
    void replaceAuthorityForGeneration(const quint64 runGeneration)
    {
        Q_EMIT authorityReplaced(runGeneration);
    }
    void finish(const quint64 operationId, const Power::CollaboratorOutcome &outcome)
    {
        Q_EMIT operationFinished(generation, operationId, outcome);
    }
    void finishForGeneration(const quint64 runGeneration, const quint64 operationId,
                             const Power::CollaboratorOutcome &outcome)
    {
        Q_EMIT operationFinished(runGeneration, operationId, outcome);
    }

    QList<ProfileOperation> profileOperations;
    int startCalls = 0;
    int stopCalls = 0;
    quint64 generation = 0;
};

class FakeSessionCollaborator final : public Power::SessionCollaborator
{
public:
    using SessionCollaborator::SessionCollaborator;

    quint64 start() override
    {
        ++startCalls;
        ++generation;
        if (generation == 0) {
            ++generation;
        }
        return generation;
    }
    void stop() override { ++stopCalls; }

    void publish(const Power::SessionFacts &facts) { publishForGeneration(generation, facts); }
    void publishForGeneration(const quint64 runGeneration,
                              const Power::SessionFacts &facts)
    {
        Q_EMIT factsChanged(runGeneration, facts);
    }
    void publishUnavailable(const QString &reasonCode = QStringLiteral("upstream-unavailable"))
    {
        Q_EMIT statusUnavailable(generation, reasonCode);
    }
    void publishUnavailableForGeneration(const quint64 runGeneration,
                                         const QString &reasonCode)
    {
        Q_EMIT statusUnavailable(runGeneration, reasonCode);
    }
    void replaceAuthority() { Q_EMIT authorityReplaced(generation); }
    void replaceAuthorityForGeneration(const quint64 runGeneration)
    {
        Q_EMIT authorityReplaced(runGeneration);
    }

    int startCalls = 0;
    int stopCalls = 0;
    quint64 generation = 0;
};

inline Power::PowerSupply fixtureSupply(const QString &opaqueId = QStringLiteral("battery-bat0"))
{
    Power::PowerSupply supply;
    supply.handle = {.epoch = 0, .opaqueId = opaqueId};
    supply.kind = Power::SupplyKind::Battery;
    supply.vendor = QStringLiteral("QindaQt");
    supply.model = QStringLiteral("Fixture cell");
    supply.present = true;
    supply.percentageKnown = true;
    supply.percentage = 55.5;
    supply.level = Power::BatteryLevel::None;
    supply.state = Power::ChargeState::Discharging;
    supply.energyKnown = true;
    supply.energyWattHours = 27.5;
    supply.energyFullWattHours = 50.0;
    supply.rateKnown = true;
    supply.energyRateWatts = 9.5;
    supply.timeToEmptyKnown = true;
    supply.timeToEmptySeconds = 10'432;
    supply.timeToFullKnown = false;
    supply.timeToFullSeconds = 0;
    supply.warning = Power::WarningLevel::Discharging;
    return supply;
}

inline Power::KeyboardBacklight fixtureKeyboard(const QString &opaqueId = QStringLiteral("keyboard-kbd0"))
{
    Power::KeyboardBacklight device;
    device.handle = {.epoch = 0, .opaqueId = opaqueId};
    device.name = QStringLiteral("Fixture keyboard backlight");
    device.valueKnown = true;
    device.value = 128;
    device.maximum = 255;
    device.normalized = 5'019;
    device.canSet = true;
    return device;
}

inline Power::BatteryFacts fixtureBatteryFacts()
{
    Power::BatteryFacts facts;
    facts.acPresent = false;
    facts.onBattery = true;
    facts.supplies = {fixtureSupply()};
    facts.keyboardBacklights = {fixtureKeyboard()};
    return facts;
}

inline Power::ProfileFacts fixtureProfileFacts()
{
    Power::ProfileFacts facts;
    facts.profiles.supported = {
        {.id = QStringLiteral("power-saver"), .label = QStringLiteral("Power Saver")},
        {.id = QStringLiteral("balanced"), .label = QStringLiteral("Balanced")},
        {.id = QStringLiteral("performance"), .label = QStringLiteral("Performance")}};
    facts.profiles.activeProfileId = QStringLiteral("balanced");
    facts.profiles.holds = {{.handle = {.epoch = 0, .opaqueId = QStringLiteral("hold-1")},
                             .profileId = QStringLiteral("balanced"),
                             .applicationName = QStringLiteral("QindaQt"),
                             .reason = QStringLiteral("fixture hold")}};
    return facts;
}

inline Power::SessionFacts fixtureSessionFacts()
{
    Power::SessionFacts facts;
    facts.lidPresent = true;
    facts.lidClosed = false;
    facts.docked = false;
    facts.preparingForSleep = false;
    facts.inhibitors = {{.what = QStringLiteral("sleep"),
                         .who = QStringLiteral("QindaQt"),
                         .why = QStringLiteral("fixture inhibitor"),
                         .mode = QStringLiteral("delay")}};
    return facts;
}

inline Power::CollaboratorOutcome succeededOutcome()
{
    return {.status = Power::CollaboratorStatus::Succeeded,
            .reasonCode = QStringLiteral("applied"),
            .diagnostic = {}};
}

} // namespace QindaQt::Tests
