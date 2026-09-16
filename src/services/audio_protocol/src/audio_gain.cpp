// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_gain.h>

#include <algorithm>
#include <cmath>

namespace QindaQt::Audio
{
namespace {

// The taper is a single power curve over the printed range. The exponent is
// chosen so unity lands near three quarters of the travel, which is where a
// mixing console puts it.
constexpr double kTaperExponent = 2.5;

[[nodiscard]] double normalizedFromDb(double gainDb)
{
    return (gainDb - kMinGainDb) / (kMaxGainDb - kMinGainDb);
}

} // namespace

double linearFromGainDb(double gainDb)
{
    const double clamped = clampGainDb(gainDb);
    if (clamped <= kMinGainDb) {
        // AGENT-GUARD: exact silence at the bottom stop. 10^(-60/20) is 0.001,
        // which is quietly audible on a loud source and reads to a user as a
        // fader that does not actually turn anything off.
        return 0.0;
    }
    return std::pow(10.0, clamped / 20.0);
}

double gainDbFromLinear(double linear)
{
    if (!std::isfinite(linear) || linear <= 0.0) {
        return kMinGainDb;
    }
    return clampGainDb(20.0 * std::log10(linear));
}

double clampGainDb(double gainDb)
{
    // AGENT-GUARD: NaN has no ordering, so it cannot be clamped - it is failed
    // QUIET, to the bottom of the scale. Sending a corrupt value to the top of
    // a gain range would be a loud fault on the user's speakers. Infinities do
    // have an ordering and clamp normally.
    if (std::isnan(gainDb)) {
        return kMinGainDb;
    }
    return std::clamp(gainDb, kMinGainDb, kMaxGainDb);
}

double gainDbFromFaderPosition(double position)
{
    if (!std::isfinite(position)) {
        return kMinGainDb;
    }
    const double clamped = std::clamp(position, 0.0, 1.0);
    const double shaped = std::pow(clamped, kTaperExponent);
    return clampGainDb(kMinGainDb + shaped * (kMaxGainDb - kMinGainDb));
}

double faderPositionFromGainDb(double gainDb)
{
    const double normalized = std::clamp(normalizedFromDb(clampGainDb(gainDb)),
                                         0.0, 1.0);
    return std::clamp(std::pow(normalized, 1.0 / kTaperExponent), 0.0, 1.0);
}

double unityFaderPosition()
{
    return faderPositionFromGainDb(kUnityGainDb);
}

PanGains panGains(const double pan)
{
    if (std::isnan(pan)) {
        return {};
    }
    const double clamped = std::clamp(pan, -1.0, 1.0);
    return {.left = clamped > 0.0 ? 1.0 - clamped : 1.0,
            .right = clamped < 0.0 ? 1.0 + clamped : 1.0};
}

} // namespace QindaQt::Audio
