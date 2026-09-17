// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_protocol/wiz_capabilities.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <algorithm>

namespace QindaQt::Wiz
{
namespace
{

// Vendor white envelope used only when a luminaire that reports a tunable
// channel set has not yet answered getModelConfig with its own range.
constexpr int defaultMinimumKelvin = 2200;
constexpr int defaultMaximumKelvin = 6500;

} // namespace

CapabilityProfile inferCapabilities(const QString &moduleName,
                                    const std::optional<ModelConfigPayload> &modelConfig)
{
    CapabilityProfile profile;
    const QString upper = moduleName.toUpper();

    // Power is the only thing a luminaire proves by answering at all. Until it
    // names its model, no channel set may be assumed: an unknown device offers
    // on/off and nothing else rather than a guessed colour wheel.
    profile.features |= Feature::Power;
    if (upper.isEmpty()) {
        profile.dimming.minimumPercent = Limits::minimumDimmingPercent;
        profile.dimming.maximumPercent = Limits::maximumDimmingPercent;
        return profile;
    }

    // A switched socket carries no light engine: power is the whole contract.
    const bool socket = upper.contains(QLatin1String("SOCKET"))
        || upper.contains(QLatin1String("PLUG"));
    if (!socket) {
        profile.features |= Feature::Dimming;
        profile.features |= Feature::Scenes;
    }

    if (upper.contains(QLatin1String("RGB"))) {
        profile.features |= Feature::Color;
        profile.features |= Feature::ColorTemperature;
        profile.features |= Feature::SceneSpeed;
    } else if (upper.contains(QLatin1String("TW"))) {
        profile.features |= Feature::ColorTemperature;
    }

    // "DH" is the vendor's dual-head prefix (an up/down luminaire); a model
    // configuration reporting more than one head is the same fact confirmed.
    const bool dualHead = upper.contains(QLatin1String("DH"))
        || (modelConfig.has_value() && modelConfig->headCount > 1);
    if (dualHead && !socket) {
        profile.features |= Feature::DualHeadRatio;
    }

    if (profile.features.testFlag(Feature::ColorTemperature)) {
        profile.temperature = {defaultMinimumKelvin, defaultMaximumKelvin};
    }

    // AGENT-NOTE (verified on firmware 1.38.0, ESP25_SHRGB_01): setPilot does
    // not refuse a temperature outside cctRange, it silently clamps to the
    // nearest endpoint. Honouring the reported range locally is what keeps the
    // projected slider and the light in agreement.
    if (modelConfig.has_value() && modelConfig->temperatureRangeKnown
        && profile.features.testFlag(Feature::ColorTemperature)) {
        profile.temperature = {modelConfig->minimumKelvin, modelConfig->maximumKelvin};
    }

    // AGENT-NOTE (same firmware): dimming below ten percent is accepted, and
    // the device's own minDimLevel is the honest floor. Older firmware answers
    // "Invalid params" below ten, so a device that has not reported a floor
    // keeps the conservative vendor minimum.
    if (modelConfig.has_value() && modelConfig->dimmingFloorKnown) {
        profile.dimming.minimumPercent =
            std::clamp(modelConfig->minimumDimmingPercent, 1,
                       Limits::maximumDimmingPercent);
    } else {
        profile.dimming.minimumPercent = Limits::minimumDimmingPercent;
    }
    profile.dimming.maximumPercent = Limits::maximumDimmingPercent;

    profile.complete = modelConfig.has_value();

    return profile;
}

} // namespace QindaQt::Wiz
