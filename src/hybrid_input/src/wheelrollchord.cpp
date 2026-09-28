// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/wheelrollchord.h"

#include <QtMath>

#include <cmath>

namespace QindaQt::HybridInput {
namespace {

// KWin reports 15 degrees per wheel notch in `delta`; a touchpad reports
// scroll units on the same scale, so one notch is 15 units of finger travel.
constexpr qreal V120PerDeltaUnit = 120.0 / 15.0;

// Positive = away from the user. KWin's vertical delta is negative for a
// wheel turned away unless natural scrolling inverted it.
qint32 awayFromUserUnits(const WheelSample &sample)
{
    const qreal raw = sample.deltaV120 != 0 ? qreal(sample.deltaV120)
                                            : sample.delta * V120PerDeltaUnit;
    return qint32(std::lround(sample.inverted ? raw : -raw));
}

} // namespace

WheelRollChord::WheelRollChord(Qt::KeyboardModifiers modifier)
    : m_modifier(modifier)
{
}

void WheelRollChord::setModifier(Qt::KeyboardModifiers modifier)
{
    m_modifier = modifier;
    reset();
}

void WheelRollChord::reset() noexcept
{
    m_inGesture = false;
    m_fired = false;
    m_accumulated = 0;
}

WheelChordDecision WheelRollChord::feed(const WheelSample &sample)
{
    if (m_modifier == Qt::NoModifier || sample.modifiers != m_modifier
        || !sample.targetUnderPointer) {
        reset();
        return {};
    }
    if (!m_inGesture || sample.timeMs - m_lastTimeMs > GesturePauseMs
        || sample.timeMs < m_lastTimeMs) {
        reset();
        m_inGesture = true;
    }
    m_lastTimeMs = sample.timeMs;

    WheelChordDecision decision{.consumed = true, .roll = std::nullopt};
    const qint32 units = awayFromUserUnits(sample);
    if (units == 0) {
        // A touchpad's stop event (or a zero sample) ends the gesture.
        reset();
        return decision;
    }
    if (m_fired) {
        return decision;
    }
    // A direction change restarts accumulation, so touchpad jitter in the
    // other direction can never add up to a notch.
    if ((units > 0) != (m_accumulated > 0) && m_accumulated != 0) {
        m_accumulated = 0;
    }
    m_accumulated += units;
    if (std::abs(m_accumulated) >= NotchUnits) {
        decision.roll = m_accumulated > 0 ? RollDirection::Up : RollDirection::Down;
        m_fired = true;
        m_accumulated = 0;
    }
    return decision;
}

} // namespace QindaQt::HybridInput
