// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QSet>
#include <QtGlobal>

#include <optional>

namespace QindaQt::HybridInput {

// ADR-0282: a finger held still on a window's own (native) title bar picks
// the window up for docking, the touch equivalent of Meta + Shift + drag.
enum class TouchPickupAction {
    None,
    // The long press fired and the adapter took the sequence over: adopt a
    // dock drag of the window at `position`.
    PickUp,
    // The picked-up finger moved; update the dock drag.
    Move,
    // The picked-up finger lifted; commit the dock drag at `position`.
    Drop,
    // A second finger (or a seat-wide cancel) abandons the dock drag.
    Cancel,
};

struct TouchPickupDecision final
{
    TouchPickupAction action = TouchPickupAction::None;
    QPointF position;
    // True when the event must not reach KWin. Only motion of a picked-up
    // finger is consumed; the lift is passed on so KWin's decoration filter
    // releases the title press it recorded on touch down.
    bool consumed = false;
    friend bool operator==(const TouchPickupDecision &, const TouchPickupDecision &) = default;
};

struct TouchPickupConfig final
{
    qint64 longPressMs = 500;
    // Movement within this radius still counts as holding still.
    qreal slop = 8.0;
    // A sequence silent this long lost its lift: the next finger starts fresh.
    qint64 staleSequenceMs = 5000;
};

// AGENT-CONTRACT: pure touch policy; the KWin adapter owns the timer, the
// title hit-test and the takeover. Until the long press fires every event of
// the sequence stays KWin's, so a tap and an ordinary title drag behave
// exactly as before: the finger moving past `slop`, a second finger, or a
// lift before the due time disarms it for the rest of the sequence. The
// takeover happens only in expire(), and only when the adapter confirms it
// can (`canTakeOver`: KWin has not started its own move and the finger is on
// the title, not on a title-bar button). After a pickup a second finger
// cancels, and the sequence is passed through until every finger lifts.
class TouchTitlePickup final
{
public:
    explicit TouchTitlePickup(TouchPickupConfig config = {});

    void setConfig(const TouchPickupConfig &config) { m_config = config; }
    [[nodiscard]] TouchPickupDecision down(qint32 id, const QPointF &position, qint64 timeMs,
                                           bool overNativeTitle);
    [[nodiscard]] TouchPickupDecision motion(qint32 id, const QPointF &position, qint64 timeMs);
    [[nodiscard]] TouchPickupDecision up(qint32 id, qint64 timeMs);
    // Seat-wide cancel: every finger is gone.
    [[nodiscard]] TouchPickupDecision cancel();
    [[nodiscard]] TouchPickupDecision expire(qint64 nowMs, bool canTakeOver);

    [[nodiscard]] std::optional<qint64> longPressDueMs() const;
    [[nodiscard]] bool armed() const noexcept { return m_state == State::Armed; }
    [[nodiscard]] bool pickedUp() const noexcept { return m_state == State::PickedUp; }
    // Any sequence this policy is following (armed, picked up, or spent).
    [[nodiscard]] bool tracking() const noexcept { return m_state != State::Idle; }
    [[nodiscard]] QPointF armedPosition() const noexcept { return m_start; }

private:
    enum class State {
        Idle,
        Armed,
        PickedUp,
        // Disarmed or cancelled: pass everything until every finger lifts.
        Spent,
    };

    void settleIfEmpty();

    TouchPickupConfig m_config;
    State m_state = State::Idle;
    QSet<qint32> m_fingers;
    qint32 m_id = -1;
    QPointF m_start;
    QPointF m_last;
    qint64 m_startMs = 0;
    qint64 m_lastEventMs = 0;
};

} // namespace QindaQt::HybridInput
