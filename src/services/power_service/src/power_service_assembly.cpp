// SPDX-License-Identifier: LGPL-3.0-or-later

#include "power_service_assembly_p.h"

#include <qindaqt/services/power_protocol/power_aggregation.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

namespace QindaQt::Power {
namespace Assembly {
namespace {

bool nonemptyBounded(const QString &value, qsizetype maximumUtf8Bytes)
{
    return !value.isEmpty() && isBoundedText(value, maximumUtf8Bytes);
}

bool validKeyboardDevice(const KeyboardBacklight &device)
{
    return isBoundedText(device.name, kMaxNameUtf8Bytes)
        && device.maximum <= kMaximumRawBrightness
        && (device.valueKnown
                ? device.maximum > 0 && device.value <= device.maximum
                      && device.normalized <= kNormalizedBrightnessMaximum
                : device.value == 0 && device.normalized == 0)
        && (!device.canSet || device.valueKnown);
}

} // namespace

bool sanitizeBatteryFacts(const BatteryFacts &input, const quint64 epoch,
                          BatteryFacts &output)
{
    if (input.supplies.size() > kMaxPowerSupplies
        || input.keyboardBacklights.size() > kMaxKeyboardBacklights) {
        return false;
    }
    BatteryFacts candidate;
    candidate.acPresent = input.acPresent;
    candidate.onBattery = input.onBattery;

    QSet<QString> ids;
    for (const PowerSupply &supply : input.supplies) {
        PowerSupply stamped = supply;
        stamped.vendor = sanitizeText(supply.vendor, kMaxNameUtf8Bytes);
        stamped.model = sanitizeText(supply.model, kMaxNameUtf8Bytes);
        stamped.handle.epoch = epoch;
        stamped.handle.opaqueId =
            sanitizeText(supply.handle.opaqueId, kMaxOpaqueIdUtf8Bytes);
        if (!nonemptyBounded(stamped.handle.opaqueId, kMaxOpaqueIdUtf8Bytes)
            || ids.contains(stamped.handle.opaqueId)
            || !validateSupply(stamped, epoch).accepted) {
            return false;
        }
        ids.insert(stamped.handle.opaqueId);
        candidate.supplies.push_back(stamped);
    }
    for (const KeyboardBacklight &device : input.keyboardBacklights) {
        KeyboardBacklight stamped = device;
        stamped.name = sanitizeText(device.name, kMaxNameUtf8Bytes);
        stamped.handle.epoch = epoch;
        stamped.handle.opaqueId =
            sanitizeText(device.handle.opaqueId, kMaxOpaqueIdUtf8Bytes);
        if (!nonemptyBounded(stamped.handle.opaqueId, kMaxOpaqueIdUtf8Bytes)
            || ids.contains(stamped.handle.opaqueId) || !validKeyboardDevice(stamped)) {
            return false;
        }
        ids.insert(stamped.handle.opaqueId);
        candidate.keyboardBacklights.push_back(stamped);
    }
    output = candidate;
    return true;
}

QSet<QString> batteryOpaqueIds(const BatteryFacts &facts)
{
    QSet<QString> ids;
    for (const PowerSupply &supply : facts.supplies) {
        ids.insert(supply.handle.opaqueId);
    }
    for (const KeyboardBacklight &device : facts.keyboardBacklights) {
        ids.insert(device.handle.opaqueId);
    }
    return ids;
}

bool sanitizeProfileFacts(const ProfileFacts &input, const quint64 epoch,
                          const QSet<QString> &reservedOpaqueIds,
                          ProfileFacts &output)
{
    const ProfileState &in = input.profiles;
    if (in.supported.size() > kMaxProfiles || in.holds.size() > kMaxProfileHolds) {
        return false;
    }
    ProfileFacts candidate;
    ProfileState &out = candidate.profiles;

    QSet<QString> profileIds;
    for (const Profile &profile : in.supported) {
        Profile sanitized = profile;
        sanitized.id = sanitizeText(profile.id, kMaxProfileIdUtf8Bytes);
        sanitized.label = sanitizeText(profile.label, kMaxNameUtf8Bytes);
        if (!nonemptyBounded(sanitized.id, kMaxProfileIdUtf8Bytes)
            || profileIds.contains(sanitized.id)) {
            return false;
        }
        profileIds.insert(sanitized.id);
        out.supported.push_back(sanitized);
    }
    out.activeProfileId = sanitizeText(in.activeProfileId, kMaxProfileIdUtf8Bytes);
    if (!out.activeProfileId.isEmpty() && !profileIds.contains(out.activeProfileId)) {
        return false;
    }
    out.degradationReason =
        sanitizeText(in.degradationReason, kMaxDiagnosticUtf8Bytes);
    for (const ProfileHold &hold : in.holds) {
        ProfileHold stamped = hold;
        stamped.profileId = sanitizeText(hold.profileId, kMaxProfileIdUtf8Bytes);
        stamped.applicationName =
            sanitizeText(hold.applicationName, kMaxNameUtf8Bytes);
        stamped.reason = sanitizeText(hold.reason, kMaxDiagnosticUtf8Bytes);
        stamped.handle.epoch = epoch;
        stamped.handle.opaqueId =
            sanitizeText(hold.handle.opaqueId, kMaxOpaqueIdUtf8Bytes);
        if (!nonemptyBounded(stamped.handle.opaqueId, kMaxOpaqueIdUtf8Bytes)
            || reservedOpaqueIds.contains(stamped.handle.opaqueId)
            || !profileIds.contains(stamped.profileId)) {
            return false;
        }
        out.holds.push_back(stamped);
    }
    output = candidate;
    return true;
}

bool sanitizeSessionFacts(const SessionFacts &input, SessionFacts &output)
{
    if (input.inhibitors.size() > kMaxInhibitors
        || (input.lidClosed && !input.lidPresent)) {
        return false;
    }
    SessionFacts candidate;
    candidate.lidPresent = input.lidPresent;
    candidate.lidClosed = input.lidClosed;
    candidate.docked = input.docked;
    candidate.preparingForSleep = input.preparingForSleep;
    for (const Inhibitor &inhibitor : input.inhibitors) {
        Inhibitor sanitized = inhibitor;
        sanitized.what = sanitizeText(inhibitor.what, kMaxInhibitorWhatUtf8Bytes);
        sanitized.who = sanitizeText(inhibitor.who, kMaxInhibitorWhoUtf8Bytes);
        sanitized.why = sanitizeText(inhibitor.why, kMaxInhibitorWhyUtf8Bytes);
        sanitized.mode = sanitizeText(inhibitor.mode, kMaxInhibitorModeUtf8Bytes);
        if (!nonemptyBounded(sanitized.what, kMaxInhibitorWhatUtf8Bytes)
            || !nonemptyBounded(sanitized.mode, kMaxInhibitorModeUtf8Bytes)) {
            return false;
        }
        candidate.inhibitors.push_back(sanitized);
    }
    output = candidate;
    return true;
}

Snapshot assembleSnapshot(const AssemblyInput &input)
{
    Snapshot snapshot;
    snapshot.protocolVersion = kProtocolVersion;
    snapshot.epoch = input.epoch;
    snapshot.revision = input.revision;

    if (input.battery != nullptr) {
        snapshot.source.acPresent = input.battery->acPresent;
        snapshot.source.onBattery = input.battery->onBattery;
        snapshot.supplies = input.battery->supplies;
        snapshot.keyboardBacklights = input.battery->keyboardBacklights;
        // AGENT-GUARD: Public handles must carry the epoch being published.
        // Stored domain facts keep their acceptance-time epoch; every assembly
        // restamps so an authority replacement invalidates all earlier handles.
        for (PowerSupply &supply : snapshot.supplies) {
            supply.handle.epoch = input.epoch;
        }
        for (KeyboardBacklight &device : snapshot.keyboardBacklights) {
            device.handle.epoch = input.epoch;
        }
    }
    if (input.profiles != nullptr) {
        snapshot.profiles = input.profiles->profiles;
        for (ProfileHold &hold : snapshot.profiles.holds) {
            hold.handle.epoch = input.epoch;
        }
    }
    if (input.session != nullptr) {
        snapshot.source.lidPresent = input.session->lidPresent;
        snapshot.source.lidClosed = input.session->lidClosed;
        snapshot.source.docked = input.session->docked;
        snapshot.source.preparingForSleep = input.session->preparingForSleep;
        snapshot.inhibitors = input.session->inhibitors;
    }
    if (input.battery != nullptr) {
        snapshot.capabilities |= Capability::Supplies | Capability::KeyboardBacklight;
    }
    if (input.profiles != nullptr) {
        snapshot.capabilities |= Capability::Profiles | Capability::ProfileHolds;
    }
    if (input.session != nullptr) {
        snapshot.capabilities |= Capability::Inhibitors | Capability::Lid;
    }

    bool aggregationFailed = false;
    if (input.battery != nullptr) {
        const AggregationResult aggregate =
            aggregatePowerSupplies(input.battery->supplies);
        if (aggregate.succeeded()) {
            snapshot.composite = aggregate.composite;
        } else {
            aggregationFailed = true;
        }
    }

    const bool batteryMalformed =
        input.batteryHas && input.battery == nullptr && !input.batteryUnavailable;
    const bool profileMalformed =
        input.profileHas && input.profiles == nullptr && !input.profileUnavailable;
    const bool sessionMalformed =
        input.sessionHas && input.session == nullptr && !input.sessionUnavailable;
    const bool collecting =
        !input.batteryHas || !input.profileHas || !input.sessionHas;
    const bool anyUnavailable = input.batteryUnavailable || input.profileUnavailable
        || input.sessionUnavailable;
    const bool anyValid = input.battery != nullptr || input.profiles != nullptr
        || input.session != nullptr;

    if (collecting) {
        snapshot.availability = Availability::Starting;
        snapshot.reasonCode = QStringLiteral("starting");
    } else if (batteryMalformed || profileMalformed || sessionMalformed) {
        snapshot.availability = Availability::Degraded;
        snapshot.reasonCode = batteryMalformed ? input.batteryReason
            : profileMalformed                    ? input.profileReason
                                                 : input.sessionReason;
    } else if (aggregationFailed) {
        snapshot.availability = Availability::Degraded;
        snapshot.reasonCode = QStringLiteral("aggregation-failed");
    } else if (anyUnavailable) {
        snapshot.availability =
            anyValid ? Availability::Degraded : Availability::Unavailable;
        snapshot.reasonCode = input.batteryUnavailable ? input.batteryReason
            : input.profileUnavailable                ? input.profileReason
                                                      : input.sessionReason;
    } else {
        snapshot.availability = Availability::Ready;
        snapshot.reasonCode = QStringLiteral("ready");
    }
    return snapshot;
}

} // namespace Assembly
} // namespace QindaQt::Power
