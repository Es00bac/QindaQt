// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchrometouchpolicy.h"

#include <QLineF>

#include <cmath>

namespace QindaQt::Compositor::KWinIntegration {

HybridChromeTouchPolicy::HybridChromeTouchPolicy(TouchPolicyConfig config)
    : m_config(config)
{
    if (m_config.longPressMs <= 0) {
        m_config.longPressMs = 500;
    }
    if (!std::isfinite(m_config.slop) || m_config.slop < 0.0) {
        m_config.slop = 8.0;
    }
    if (!std::isfinite(m_config.swipeDistance) || m_config.swipeDistance <= 0.0) {
        m_config.swipeDistance = 40.0;
    }
    if (m_config.staleSequenceMs <= 0) {
        m_config.staleSequenceMs = 5000;
    }
}

TouchDecision HybridChromeTouchPolicy::down(qint32 id, const QPointF &position, qint64 timeMs,
                                            bool overChrome, bool rollTarget)
{
    if (isStale(timeMs)) {
        // Nothing arrived for the whole stale window: the grab went elsewhere
        // and no up or cancel followed. Do not eat every later finger.
        reset();
    }
    if (m_primary) {
        if (id == m_primary->id) {
            m_lastEventMs = timeMs;
            return {TouchGesture::None, position, true};
        }
        if (!overChrome) {
            // A finger on a client, a panel or another screen is KWin's: it
            // neither joins nor spends this gesture.
            return {TouchGesture::None, position, false};
        }
        m_lastEventMs = timeMs;
        // A second finger joins the gesture: it can only become a swipe now,
        // never a click or a drag, and it is never a long press.
        if (!m_secondary && !m_finished && !m_moved && m_rollTarget && !m_primaryLifted) {
            m_secondary = Finger{id, position, position};
            m_longPressArmed = false;
        }
        m_finished = m_finished || !m_rollTarget || m_moved;
        m_longPressArmed = false;
        return {TouchGesture::None, position, true};
    }
    if (!overChrome) {
        return {TouchGesture::None, position, false};
    }
    m_primary = Finger{id, position, position};
    m_secondary.reset();
    m_pressTimeMs = timeMs;
    m_lastEventMs = timeMs;
    m_longPressArmed = true;
    m_rollTarget = rollTarget;
    m_moved = false;
    m_finished = false;
    m_primaryLifted = false;
    return {TouchGesture::Press, position, true};
}

TouchDecision HybridChromeTouchPolicy::motion(qint32 id, const QPointF &position, qint64 timeMs)
{
    if (!m_primary) {
        return {TouchGesture::None, position, false};
    }
    Finger *finger = (id == m_primary->id && !m_primaryLifted) ? &*m_primary
        : (m_secondary && id == m_secondary->id) ? &*m_secondary : nullptr;
    if (finger == nullptr) {
        // Not a finger of this gesture: KWin's.
        return {TouchGesture::None, position, false};
    }
    m_lastEventMs = timeMs;
    finger->last = position;
    if (m_finished) {
        return {TouchGesture::None, position, true};
    }
    if (m_secondary) {
        const qreal dy = position.y() - finger->origin.y();
        if (std::abs(dy) >= m_config.swipeDistance) {
            m_finished = true;
            m_longPressArmed = false;
            return {dy < 0.0 ? TouchGesture::SwipeUp : TouchGesture::SwipeDown, position, true};
        }
        return {TouchGesture::None, position, true};
    }
    if (!m_moved && QLineF(m_primary->origin, position).length() > m_config.slop) {
        m_moved = true;
        m_longPressArmed = false;
    }
    return {TouchGesture::Move, position, true};
}

TouchDecision HybridChromeTouchPolicy::up(qint32 id, qint64 timeMs)
{
    if (!m_primary) {
        return {TouchGesture::None, {}, false};
    }
    if (m_secondary && id == m_secondary->id) {
        m_lastEventMs = timeMs;
        m_secondary.reset();
        // Two fingers were down: the sequence is spent even if no swipe
        // completed, so the remaining finger cannot drag or click.
        m_finished = true;
        if (m_primaryLifted) {
            // The primary already lifted: every finger is up now.
            reset();
        }
        return {TouchGesture::None, {}, true};
    }
    if (id != m_primary->id || m_primaryLifted) {
        // Not a finger of this gesture: KWin's.
        return {TouchGesture::None, {}, false};
    }
    m_lastEventMs = timeMs;
    if (m_secondary) {
        // The primary lifts first while the secondary is still down: the
        // sequence is spent (never a click), and it stays owned until the
        // secondary lifts too, so its remaining events are consumed rather
        // than leaking to the window underneath without their down.
        m_finished = true;
        m_longPressArmed = false;
        m_primaryLifted = true;
        return {TouchGesture::None, {}, true};
    }
    const QPointF last = m_primary->last;
    const bool finished = m_finished;
    reset();
    return {finished ? TouchGesture::None : TouchGesture::Release, last, true};
}

TouchDecision HybridChromeTouchPolicy::cancel()
{
    const bool wasActive = m_primary.has_value();
    reset();
    return {TouchGesture::Cancel, {}, wasActive};
}

std::optional<TouchDecision> HybridChromeTouchPolicy::expire(qint64 nowMs)
{
    if (!m_primary || !m_longPressArmed || m_moved || m_finished) {
        return std::nullopt;
    }
    if (nowMs - m_pressTimeMs < m_config.longPressMs) {
        return std::nullopt;
    }
    m_longPressArmed = false;
    m_finished = true;
    return TouchDecision{TouchGesture::LongPress, m_primary->last, true};
}

std::optional<qint64> HybridChromeTouchPolicy::longPressDueMs() const
{
    if (!m_primary || !m_longPressArmed) {
        return std::nullopt;
    }
    return m_pressTimeMs + m_config.longPressMs;
}

void HybridChromeTouchPolicy::reset()
{
    m_primary.reset();
    m_secondary.reset();
    m_pressTimeMs = 0;
    m_lastEventMs = 0;
    m_longPressArmed = false;
    m_rollTarget = false;
    m_moved = false;
    m_finished = false;
    m_primaryLifted = false;
}

} // namespace QindaQt::Compositor::KWinIntegration
