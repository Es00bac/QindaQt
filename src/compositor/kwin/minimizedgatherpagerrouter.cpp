// SPDX-License-Identifier: GPL-3.0-or-later
#include "minimizedgatherpager.h"

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

MinimizedGatherPagerRouter::MinimizedGatherPagerRouter(HitTest hitTest)
    : m_hitTest(std::move(hitTest))
{
}

bool MinimizedGatherPagerRouter::pointerPress(const QPointF &position,
                                               const bool leftButton)
{
    if (active()) {
        return true;
    }
    if (!leftButton || !m_hitTest) {
        return false;
    }
    m_pressed = m_hitTest(position);
    return m_pressed.has_value();
}

bool MinimizedGatherPagerRouter::pointerMove(const QPointF &) const noexcept
{
    return pointerActive();
}

bool MinimizedGatherPagerRouter::pointerActive() const noexcept
{
    return m_pressed.has_value() && !m_touchId.has_value();
}

std::optional<MinimizedPagerHit> MinimizedGatherPagerRouter::pointerRelease(
    const QPointF &position)
{
    if (!pointerActive()) {
        return std::nullopt;
    }
    return release(position);
}

bool MinimizedGatherPagerRouter::touchDown(const qint32 id,
                                            const QPointF &position)
{
    if (active() || !m_hitTest) {
        return active();
    }
    m_pressed = m_hitTest(position);
    if (!m_pressed) {
        return false;
    }
    m_touchId = id;
    return true;
}

bool MinimizedGatherPagerRouter::touchMotion(const qint32 id,
                                              const QPointF &) const noexcept
{
    return touchActive(id);
}

bool MinimizedGatherPagerRouter::touchActive(const qint32 id) const noexcept
{
    return m_touchId == id;
}

std::optional<MinimizedPagerHit> MinimizedGatherPagerRouter::touchUp(
    const qint32 id, const QPointF &position)
{
    if (m_touchId != id || !m_pressed) {
        return std::nullopt;
    }
    m_touchId.reset();
    return release(position);
}

void MinimizedGatherPagerRouter::cancel() noexcept
{
    m_pressed.reset();
    m_touchId.reset();
}

bool MinimizedGatherPagerRouter::active() const noexcept
{
    return m_pressed.has_value();
}

std::optional<MinimizedPagerHit> MinimizedGatherPagerRouter::release(
    const QPointF &position)
{
    const auto pressed = std::exchange(m_pressed, std::nullopt);
    if (!pressed || !m_hitTest) {
        return std::nullopt;
    }
    const auto released = m_hitTest(position);
    return released == pressed ? released : std::nullopt;
}

} // namespace QindaQt::Compositor::KWinIntegration
