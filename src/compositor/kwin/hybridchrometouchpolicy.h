// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QtGlobal>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// What one touch event over shared chrome means (ADR-0193). `Press`, `Move`
// and `Release` are the finger standing in for the left button; `LongPress`
// is the finger's right click; the swipes are the finger's wheel.
enum class TouchGesture {
    None,
    Press,
    Move,
    Release,
    LongPress,
    SwipeUp,
    SwipeDown,
    Cancel,
};

struct TouchDecision final
{
    TouchGesture gesture = TouchGesture::None;
    QPointF position;
    // True when the touch belongs to a chrome gesture and must not reach the
    // window underneath, whatever the gesture value.
    bool consumed = false;
    friend bool operator==(const TouchDecision &, const TouchDecision &) = default;
};

struct TouchPolicyConfig final
{
    // A finger held still this long opens the context menu.
    qint64 longPressMs = 500;
    // Movement within this radius still counts as holding still.
    qreal slop = 8.0;
    // A second finger travelling this far vertically is a roll-up swipe.
    qreal swipeDistance = 40.0;
    // A sequence with no event for this long is abandoned: the compositor
    // took the grab and no up or cancel ever arrived. The next finger starts
    // fresh instead of being eaten forever.
    qint64 staleSequenceMs = 5000;
};

// AGENT-CONTRACT: the pure touch policy for compositor-painted chrome. It sees
// finger ids, positions and times, and answers what the owner should do; it
// knows nothing about KWin, the router or timers, so the semantics are
// testable with plain calls. One gesture at a time: the first finger that
// lands over chrome owns it until every finger of that gesture lifts, in any
// order. A second finger joins the gesture only when it also lands over the
// chrome the gesture owns; a finger anywhere else belongs to KWin and is never
// consumed. After a long press, a swipe, or a two-finger sequence (however the
// fingers lift) the rest of that sequence is consumed silently, so a lift
// never turns into a click and a swipe never becomes a drag. A sequence that
// receives nothing for `staleSequenceMs` is abandoned on the next finger.
class HybridChromeTouchPolicy final
{
public:
    explicit HybridChromeTouchPolicy(TouchPolicyConfig config = {});

    // `overChrome` says whether this finger landed on a chrome target the
    // router owns (for a joining finger: the same container's chrome);
    // `rollTarget` whether that target rolls up on a wheel, which is what
    // makes a two-finger swipe meaningful there.
    [[nodiscard]] TouchDecision down(qint32 id, const QPointF &position, qint64 timeMs,
                                     bool overChrome, bool rollTarget);
    [[nodiscard]] TouchDecision motion(qint32 id, const QPointF &position, qint64 timeMs);
    [[nodiscard]] TouchDecision up(qint32 id, qint64 timeMs);
    [[nodiscard]] TouchDecision cancel();
    // The long-press check the owner runs when longPressDueMs() comes due.
    [[nodiscard]] std::optional<TouchDecision> expire(qint64 nowMs);

    [[nodiscard]] bool active() const noexcept { return m_primary.has_value(); }
    // True when a gesture is open but nothing has arrived for the stale
    // window: the owner should abandon it (cancel) before the next finger.
    [[nodiscard]] bool isStale(qint64 nowMs) const noexcept
    {
        return m_primary.has_value() && nowMs - m_lastEventMs > m_config.staleSequenceMs;
    }
    [[nodiscard]] std::optional<qint64> longPressDueMs() const;
    [[nodiscard]] const TouchPolicyConfig &config() const noexcept { return m_config; }

private:
    struct Finger {
        qint32 id = 0;
        QPointF origin;
        QPointF last;
    };

    void reset();

    TouchPolicyConfig m_config;
    std::optional<Finger> m_primary;
    std::optional<Finger> m_secondary;
    qint64 m_pressTimeMs = 0;
    qint64 m_lastEventMs = 0;
    bool m_longPressArmed = false;
    bool m_rollTarget = false;
    bool m_moved = false;
    bool m_finished = false;
    // The primary lifted while a secondary was still down: the sequence is
    // spent and stays owned until that secondary lifts too.
    bool m_primaryLifted = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
