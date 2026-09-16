// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Audio
{

// AGENT-CONTRACT: the one gain law shared by the audio service, every console
// fader, and the routing matrix (ADR-0171). A mixing console is operated in
// DECIBELS, not in a 0..1 scalar: users reason about "-6 dB", read numeric
// fader legends, and expect the same number to mean the same loudness on a
// strip, a bus and a matrix send. Defining the mapping once here is what keeps
// those three agreeing.
//
// Pure and header-free of Qt types beyond qreal so the protocol, the service
// worker, and QML can all use it.

// Fader travel. -60 dB is the bottom of the printed scale; anything at or below
// it is silence, which is how a hardware fader behaves at its bottom stop.
// +12 dB of makeup gain matches the reference console's range.
inline constexpr double kMinGainDb = -60.0;
inline constexpr double kMaxGainDb = 12.0;
inline constexpr double kUnityGainDb = 0.0;

// dB <-> linear amplitude. `linearFromGainDb(kMinGainDb)` is exactly 0.0, not
// 0.001: a fader pulled to the bottom must be silent, and a near-zero linear
// value would leave an audible residue on a loud source.
[[nodiscard]] double linearFromGainDb(double gainDb);
// Inverse. A linear value of 0 (or below) maps to kMinGainDb rather than
// negative infinity, so round-tripping never produces a non-finite fader.
[[nodiscard]] double gainDbFromLinear(double linear);

// Clamps to the printed scale. NaN fails QUIET to kMinGainDb rather than
// propagating into the graph or the UI - a corrupt value must never arrive as
// full gain on the user's speakers. Infinities clamp normally.
[[nodiscard]] double clampGainDb(double gainDb);

// Fader taper: position 0..1 along the slider <-> dB. The scale is NOT linear
// in dB, because a linear-in-dB fader spends most of its travel in a range
// nobody mixes in. This taper gives unity gain a fixed, findable position and
// devotes the upper travel to the working range, matching the reference
// console's feel.
//
// AGENT-GUARD: `faderPositionFromGainDb(gainDbFromFaderPosition(p)) == p` for
// every p in [0, 1] within 1e-9. A UI that drags a fader and reads the value
// back must not drift.
[[nodiscard]] double gainDbFromFaderPosition(double position);
[[nodiscard]] double faderPositionFromGainDb(double gainDb);
// The position unity gain sits at, so a console can draw its 0 dB line without
// re-deriving the taper.
[[nodiscard]] double unityFaderPosition();

} // namespace QindaQt::Audio
