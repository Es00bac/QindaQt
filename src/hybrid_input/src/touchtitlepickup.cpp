// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/touchtitlepickup.h"

#include <QLineF>

namespace QindaQt::HybridInput {

TouchTitlePickup::TouchTitlePickup(TouchPickupConfig config)
    : m_config(config)
{
}

void TouchTitlePickup::settleIfEmpty()
{
    if (m_fingers.isEmpty()) {
        m_state = State::Idle;
        m_id = -1;
    }
}

TouchPickupDecision TouchTitlePickup::down(qint32 id, const QPointF &position, qint64 timeMs,
                                           bool overNativeTitle)
{
    if (!m_fingers.isEmpty() && timeMs - m_lastEventMs > m_config.staleSequenceMs) {
        // The last sequence never reported its lift; start fresh.
        m_fingers.clear();
        m_state = State::Idle;
        m_id = -1;
    }
    m_lastEventMs = timeMs;
    const bool first = m_fingers.isEmpty();
    m_fingers.insert(id);
    switch (m_state) {
    case State::Idle:
        if (first && overNativeTitle) {
            m_state = State::Armed;
            m_id = id;
            m_start = m_last = position;
            m_startMs = timeMs;
        } else {
            m_state = State::Spent;
        }
        return {};
    case State::Armed:
        m_state = State::Spent;
        return {};
    case State::PickedUp:
        m_state = State::Spent;
        return {.action = TouchPickupAction::Cancel, .position = m_last, .consumed = false};
    case State::Spent:
        return {};
    }
    return {};
}

TouchPickupDecision TouchTitlePickup::motion(qint32 id, const QPointF &position, qint64 timeMs)
{
    m_lastEventMs = timeMs;
    if (id != m_id) {
        return {};
    }
    if (m_state == State::Armed) {
        if (QLineF(m_start, position).length() > m_config.slop) {
            // An ordinary title drag: KWin's own move takes it.
            m_state = State::Spent;
        } else {
            m_last = position;
        }
        return {};
    }
    if (m_state == State::PickedUp) {
        m_last = position;
        return {.action = TouchPickupAction::Move, .position = position, .consumed = true};
    }
    return {};
}

TouchPickupDecision TouchTitlePickup::up(qint32 id, qint64 timeMs)
{
    m_lastEventMs = timeMs;
    m_fingers.remove(id);
    TouchPickupDecision decision;
    if (id == m_id && m_state == State::PickedUp) {
        decision = {.action = TouchPickupAction::Drop, .position = m_last, .consumed = false};
        m_state = State::Spent;
    } else if (id == m_id && m_state == State::Armed) {
        // A tap: KWin already has every event of it.
        m_state = State::Spent;
    }
    settleIfEmpty();
    return decision;
}

TouchPickupDecision TouchTitlePickup::cancel()
{
    const bool wasPickedUp = m_state == State::PickedUp;
    const QPointF last = m_last;
    m_fingers.clear();
    m_state = State::Idle;
    m_id = -1;
    return wasPickedUp
        ? TouchPickupDecision{.action = TouchPickupAction::Cancel, .position = last, .consumed = false}
        : TouchPickupDecision{};
}

std::optional<qint64> TouchTitlePickup::longPressDueMs() const
{
    return m_state == State::Armed ? std::optional<qint64>(m_startMs + m_config.longPressMs)
                                   : std::nullopt;
}

TouchPickupDecision TouchTitlePickup::expire(qint64 nowMs, bool canTakeOver)
{
    if (m_state != State::Armed || nowMs < m_startMs + m_config.longPressMs) {
        return {};
    }
    if (!canTakeOver) {
        m_state = State::Spent;
        return {};
    }
    m_state = State::PickedUp;
    return {.action = TouchPickupAction::PickUp,
            .feedback = TouchPickupFeedback::Lift,
            .position = m_last,
            .consumed = true};
}

} // namespace QindaQt::HybridInput
