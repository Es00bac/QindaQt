// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>
#include <Qt>

#include <optional>

namespace QindaQt::HybridInput {

enum class RollDirection {
    // Wheel turned away from the user: roll up (shade / roll to the icon).
    Up,
    // Towards the user: roll back down.
    Down,
};

struct WheelSample final
{
    qint64 timeMs = 0;
    Qt::KeyboardModifiers modifiers;
    // KWin's PointerAxisEvent values for a vertical axis: `delta` in wheel
    // degrees (15 per notch) or touchpad scroll units, `deltaV120` in 1/120
    // notches (0 for a touchpad), `inverted` for natural scrolling.
    qreal delta = 0.0;
    qint32 deltaV120 = 0;
    bool inverted = false;
    // Something the chord can act on is under the pointer (a managed window,
    // container chrome, or an icon chip). Over the bare desktop or a panel the
    // chord stays out of the way so KWin's own Meta+wheel (zoom) still works.
    bool targetUnderPointer = false;
};

struct WheelChordDecision final
{
    bool consumed = false;
    std::optional<RollDirection> roll;
};

// ADR-0282: modifier + wheel rolls the container under the pointer up or
// down. One notch (120 units) fires once per scroll gesture: a high-resolution
// wheel or a touchpad accumulates to a notch, and the rest of that gesture is
// swallowed, so a smooth scroll never flaps the container up and down. A
// gesture ends after a pause or on a touchpad's zero-delta stop event.
//
// AGENT-CONTRACT: every sample with the exact modifier over a target is
// consumed, fired or not, so neither the plain-wheel tab-strip scroll nor
// KWin's axis shortcuts see a chord the user meant for roll-up. Idempotence
// ("roll up" on a rolled-up container is a no-op) is the caller's; this class
// only decides direction and timing.
class WheelRollChord final
{
public:
    static constexpr qint32 NotchUnits = 120;
    static constexpr qint64 GesturePauseMs = 300;

    explicit WheelRollChord(Qt::KeyboardModifiers modifier = Qt::MetaModifier);

    // NoModifier disables the chord.
    void setModifier(Qt::KeyboardModifiers modifier);
    [[nodiscard]] Qt::KeyboardModifiers modifier() const noexcept { return m_modifier; }
    [[nodiscard]] WheelChordDecision feed(const WheelSample &sample);
    void reset() noexcept;

private:
    Qt::KeyboardModifiers m_modifier;
    qint64 m_lastTimeMs = 0;
    bool m_inGesture = false;
    bool m_fired = false;
    qint32 m_accumulated = 0;
};

} // namespace QindaQt::HybridInput
