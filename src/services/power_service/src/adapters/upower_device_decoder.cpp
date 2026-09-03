// SPDX-License-Identifier: GPL-3.0-or-later

#include "upower_device_decoder_p.h"

#include "upstream_identity.h"

#include <cmath>

namespace QindaQt::Power::Upstream {
namespace {

constexpr uint kLinePower = 1;
constexpr uint kBattery = 2;
constexpr uint kUps = 3;

template <typename Value>
bool optionalExact(const QVariantMap &properties, const QString &name,
                   const QMetaType type, Value &value, bool &present)
{
    const auto it = properties.constFind(name);
    present = it != properties.constEnd();
    if (!present) {
        return true;
    }
    if (it.value().metaType() != type) {
        return false;
    }
    value = it.value().value<Value>();
    return true;
}

bool mapChargeState(const uint raw, ChargeState &state)
{
    switch (raw) {
    case 0: state = ChargeState::Unknown; return true;
    case 1: state = ChargeState::Charging; return true;
    case 2: state = ChargeState::Discharging; return true;
    case 3: state = ChargeState::Empty; return true;
    case 4: state = ChargeState::FullyCharged; return true;
    case 5: state = ChargeState::PendingCharge; return true;
    case 6: state = ChargeState::PendingDischarge; return true;
    default: return false;
    }
}

bool mapBatteryLevel(const uint raw, BatteryLevel &level)
{
    switch (raw) {
    case 0: level = BatteryLevel::Unknown; return true;
    case 1: level = BatteryLevel::None; return true;
    case 2: level = BatteryLevel::Low; return true;
    case 3: level = BatteryLevel::Critical; return true;
    case 4: level = BatteryLevel::Normal; return true;
    case 5: level = BatteryLevel::High; return true;
    case 6: level = BatteryLevel::Full; return true;
    default: return false;
    }
}

bool mapWarningLevel(const uint raw, WarningLevel &warning)
{
    switch (raw) {
    case 0: warning = WarningLevel::Unknown; return true;
    case 1: warning = WarningLevel::None; return true;
    case 2: warning = WarningLevel::Discharging; return true;
    case 3: warning = WarningLevel::Low; return true;
    case 4: warning = WarningLevel::Critical; return true;
    case 5: warning = WarningLevel::Action; return true;
    default: return false;
    }
}

bool decodeText(const QVariantMap &properties, const QString &name, QString &value)
{
    bool present = false;
    return optionalExact(properties, name, QMetaType::fromType<QString>(), value,
                         present);
}

} // namespace

bool decodeUpowerDevice(const QString &objectPath, const QVariantMap &properties,
                        UpowerDeviceTruth &truth)
{
    uint type = 0;
    bool hasType = false;
    if (!optionalExact(properties, QStringLiteral("Type"),
                       QMetaType::fromType<uint>(), type, hasType)
        || !hasType) {
        return false;
    }

    UpowerDeviceTruth candidate;
    if (type != kLinePower && type != kBattery && type != kUps) {
        truth = candidate;
        return true;
    }

    bool present = false;
    bool hasPresent = false;
    if (!optionalExact(properties, QStringLiteral("IsPresent"),
                       QMetaType::fromType<bool>(), present, hasPresent)
        || !hasPresent) {
        return false;
    }
    if (type == kLinePower) {
        candidate.acPresent = present;
        truth = candidate;
        return true;
    }

    PowerSupply supply;
    supply.handle.opaqueId =
        deriveOpaqueId(QStringLiteral("upower-supply"), objectPath);
    supply.kind = type == kBattery ? SupplyKind::Battery : SupplyKind::Ups;
    supply.present = present;
    if (!decodeText(properties, QStringLiteral("Vendor"), supply.vendor)
        || !decodeText(properties, QStringLiteral("Model"), supply.model)) {
        return false;
    }

    uint rawState = 0;
    bool hasState = false;
    if (!optionalExact(properties, QStringLiteral("State"),
                       QMetaType::fromType<uint>(), rawState, hasState)
        || (hasState && !mapChargeState(rawState, supply.state))) {
        return false;
    }
    uint rawWarning = 0;
    bool hasWarning = false;
    if (!optionalExact(properties, QStringLiteral("WarningLevel"),
                       QMetaType::fromType<uint>(), rawWarning, hasWarning)
        || (hasWarning && !mapWarningLevel(rawWarning, supply.warning))) {
        return false;
    }
    uint rawLevel = 0;
    bool hasLevel = false;
    BatteryLevel level = BatteryLevel::Unknown;
    if (!optionalExact(properties, QStringLiteral("BatteryLevel"),
                       QMetaType::fromType<uint>(), rawLevel, hasLevel)
        || (hasLevel && !mapBatteryLevel(rawLevel, level))) {
        return false;
    }

    double percentage = 0.0;
    bool hasPercentage = false;
    if (!optionalExact(properties, QStringLiteral("Percentage"),
                       QMetaType::fromType<double>(), percentage, hasPercentage)) {
        return false;
    }
    if (hasPercentage && (!hasLevel || rawLevel <= 1)) {
        supply.percentageKnown = true;
        supply.percentage = percentage;
        supply.level = BatteryLevel::None;
    } else if (hasLevel) {
        supply.level = level;
    }

    double energy = 0.0;
    double energyFull = 0.0;
    bool hasEnergy = false;
    bool hasEnergyFull = false;
    if (!optionalExact(properties, QStringLiteral("Energy"),
                       QMetaType::fromType<double>(), energy, hasEnergy)
        || !optionalExact(properties, QStringLiteral("EnergyFull"),
                          QMetaType::fromType<double>(), energyFull,
                          hasEnergyFull)
        || hasEnergy != hasEnergyFull) {
        return false;
    }
    if (hasEnergy && energyFull > 0.0) {
        supply.energyKnown = true;
        supply.energyWattHours = energy;
        supply.energyFullWattHours = energyFull;
    }

    double rate = 0.0;
    bool hasRate = false;
    if (!optionalExact(properties, QStringLiteral("EnergyRate"),
                       QMetaType::fromType<double>(), rate, hasRate)) {
        return false;
    }
    if (hasRate) {
        // UPower permits a signed rate while Power1 stores an absolute
        // per-supply rate and derives aggregate direction from ChargeState.
        supply.rateKnown = true;
        supply.energyRateWatts = std::abs(rate);
    }

    qint64 timeToEmpty = 0;
    qint64 timeToFull = 0;
    bool hasTimeToEmpty = false;
    bool hasTimeToFull = false;
    if (!optionalExact(properties, QStringLiteral("TimeToEmpty"),
                       QMetaType::fromType<qint64>(), timeToEmpty,
                       hasTimeToEmpty)
        || !optionalExact(properties, QStringLiteral("TimeToFull"),
                          QMetaType::fromType<qint64>(), timeToFull,
                          hasTimeToFull)) {
        return false;
    }
    if (hasTimeToEmpty && timeToEmpty != 0) {
        supply.timeToEmptyKnown = true;
        supply.timeToEmptySeconds = timeToEmpty;
    }
    if (hasTimeToFull && timeToFull != 0) {
        supply.timeToFullKnown = true;
        supply.timeToFullSeconds = timeToFull;
    }

    candidate.hasSupply = true;
    candidate.supply = std::move(supply);
    truth = std::move(candidate);
    return true;
}

} // namespace QindaQt::Power::Upstream
