// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Audio
{

// Private amplitude declaration. Each stage has its own bottom-stop; folding
// them into one clamped dB value revives a silent strip or drops a quiet send.
struct ConsoleSendGain {
    double stripDb = 0.0;
    double sendDb = 0.0;
    double busDb = 0.0;
    double pan = 0.0;
    bool audible = true;
};

struct ConsoleSendAmplitude {
    double left = 0.0;
    double right = 0.0;
    bool valid = false;
};

// Invalid private declarations fail quiet. Public request range admission is
// still owned by AudioProtocol/coordinator; this is no alternative grant.
[[nodiscard]] ConsoleSendAmplitude consoleSendAmplitude(ConsoleSendGain gain) noexcept;
// Mixer API scale remains cubic for ordinary percentages. Only an admitted
// internal send uses this inverse conversion from desired PCM amplitude.
[[nodiscard]] double consoleMixerScalar(double amplitude) noexcept;
// Derived three-stage headroom, rounded upward for PipeWire float Props.
[[nodiscard]] float consoleSendMaximumAmplitude() noexcept;

} // namespace QindaQt::Audio
