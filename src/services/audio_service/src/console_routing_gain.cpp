// SPDX-License-Identifier: GPL-3.0-or-later
#include "console_routing_gain_p.h"

#include <qindaqt/services/audio_protocol/audio_gain.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <cmath>
#include <limits>

namespace QindaQt::Audio
{
namespace
{
bool admittedStage(const double gain) noexcept
{
    return std::isfinite(gain) && gain >= kMinGainDb && gain <= kMaxGainDb;
}
}

ConsoleSendAmplitude consoleSendAmplitude(const ConsoleSendGain gain) noexcept
{
    if (!admittedStage(gain.stripDb) || !admittedStage(gain.sendDb)
        || !admittedStage(gain.busDb) || !std::isfinite(gain.pan)
        || gain.pan < -1.0 || gain.pan > 1.0) {
        return {};
    }
    const double amplitude = gain.audible
        ? linearFromGainDb(gain.stripDb) * linearFromGainDb(gain.sendDb)
            * linearFromGainDb(gain.busDb)
        : 0.0;
    const PanGains pan = panGains(gain.pan);
    return {.left = amplitude * pan.left, .right = amplitude * pan.right,
            .valid = true};
}

double consoleMixerScalar(const double amplitude) noexcept
{
    if (!std::isfinite(amplitude) || amplitude < 0.0
        || amplitude > static_cast<double>(consoleSendMaximumAmplitude())) {
        return 0.0;
    }
    return std::cbrt(amplitude);
}

float consoleSendMaximumAmplitude() noexcept
{
    const double stage = linearFromGainDb(kMaxGainDb);
    const float bound = static_cast<float>(stage * stage * stage);
    // AGENT-GUARD: PipeWire clamps each channel to this bound. An ordinary
    // 0..1/public percent cap or its default10 would change admitted +36dB.
    return std::nextafter(bound, std::numeric_limits<float>::infinity());
}

} // namespace QindaQt::Audio
