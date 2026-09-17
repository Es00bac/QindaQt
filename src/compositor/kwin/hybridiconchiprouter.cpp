// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridiconchiprouter.h"

#include <QLineF>

#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr qreal DefaultDragThreshold = 8.0;
constexpr qreal DefaultDoubleClickIntervalMs = 400.0;

using HybridChrome::IconChipHitKind;

} // namespace

bool hasIconChipDecisionOutput(const IconChipPointerDecision &decision) noexcept
{
    return decision.hoverChanged || !decision.raiseRequests.isEmpty()
        || !decision.unrollRequests.isEmpty() || !decision.closeRequests.isEmpty()
        || !decision.drags.isEmpty() || !decision.contextMenus.isEmpty();
}

HybridIconChipRouter::HybridIconChipRouter(HitResolver resolver,
                                           qreal dragThreshold,
                                           qreal doubleClickIntervalMs)
    : m_resolver(std::move(resolver))
    , m_dragThreshold(dragThreshold)
    , m_doubleClickIntervalMs(doubleClickIntervalMs)
{
    if (!std::isfinite(m_dragThreshold) || m_dragThreshold < 0.0) {
        m_dragThreshold = DefaultDragThreshold;
    }
    if (!std::isfinite(m_doubleClickIntervalMs) || m_doubleClickIntervalMs <= 0.0) {
        m_doubleClickIntervalMs = DefaultDoubleClickIntervalMs;
    }
    m_clickClock.start();
}

std::optional<IconChipPointerHit> HybridIconChipRouter::hitAt(const QPointF &position) const
{
    if (!m_resolver) {
        return std::nullopt;
    }
    auto hit = m_resolver(position);
    return hit && hit->isValid() ? std::move(hit) : std::nullopt;
}

void HybridIconChipRouter::updateHover(const QPointF &position,
                                       IconChipPointerDecision *decision)
{
    const auto next = hitAt(position);
    if (next == m_hovered) {
        return;
    }
    m_hovered = next;
    decision->hoverChanged = true;
    decision->hovered = next;
}

void HybridIconChipRouter::appendDrag(HybridChrome::DragPhase phase,
                                      const QPointF &position,
                                      IconChipPointerDecision *decision) const
{
    if (!m_pressed) {
        return;
    }
    decision->drags.append({m_pressed->windowId, phase, position,
                            position - m_pressPosition});
}

void HybridIconChipRouter::resetPointer() noexcept
{
    m_pressed.reset();
    m_pressPosition = {};
    m_pressedButton = Qt::NoButton;
    m_dragActive = false;
}

void HybridIconChipRouter::cancelPointer(IconChipPointerDecision *decision)
{
    if (!m_pressed) {
        return;
    }
    decision->consumed = true;
    if (m_dragActive) {
        appendDrag(HybridChrome::DragPhase::Cancel, m_lastPosition, decision);
    }
    resetPointer();
}

IconChipPointerDecision HybridIconChipRouter::pointerMove(
    const HybridInput::PointerEvent &event)
{
    IconChipPointerDecision decision;
    updateHover(event.position, &decision);
    const auto previousPosition = m_lastPosition;
    m_lastPosition = event.position;
    if (!m_pressed) {
        return decision;
    }
    decision.consumed = true;
    if (!event.buttons.testFlag(m_pressedButton)) {
        // AGENT-GUARD: a lost release must terminate the grab, or every later
        // client motion would be swallowed indefinitely.
        cancelPointer(&decision);
        return decision;
    }
    if (m_pressedButton != Qt::LeftButton
        || m_pressed->target != IconChipHitKind::Body) {
        return decision;
    }
    if (!m_dragActive
        && QLineF(m_pressPosition, event.position).length() >= m_dragThreshold) {
        m_dragActive = true;
        appendDrag(HybridChrome::DragPhase::Begin, event.position, &decision);
    } else if (m_dragActive && event.position != previousPosition) {
        appendDrag(HybridChrome::DragPhase::Update, event.position, &decision);
    }
    return decision;
}

IconChipPointerDecision HybridIconChipRouter::pointerPress(
    const HybridInput::PointerEvent &event)
{
    IconChipPointerDecision decision;
    updateHover(event.position, &decision);
    m_lastPosition = event.position;
    if (event.changedButton != Qt::LeftButton && event.changedButton != Qt::RightButton) {
        return decision;
    }
    if (m_pressed) {
        cancelPointer(&decision);
    }
    // AGENT-CONTRACT: any modifier passes through. That is what lets the
    // exact Meta+Shift+Left chord reach the InteractionController, which
    // resolves the chip as a dock source through the target resolver.
    if (event.modifiers != Qt::NoModifier
        || !event.buttons.testFlag(event.changedButton) || !m_hovered) {
        return decision;
    }
    m_pressed = m_hovered;
    m_pressPosition = event.position;
    m_pressedButton = event.changedButton;
    m_dragActive = false;
    decision.consumed = true;
    if (event.changedButton == Qt::LeftButton
        && m_pressed->target == IconChipHitKind::Body) {
        decision.raiseRequests.append(m_pressed->windowId);
    }
    return decision;
}

void HybridIconChipRouter::noteClick(const IconChipPointerHit &hit)
{
    m_lastClickMs = m_clickClock.isValid() ? m_clickClock.elapsed() : 0;
    m_lastClickWindow = hit.windowId;
}

bool HybridIconChipRouter::isDoubleClick(const IconChipPointerHit &hit) const
{
    return m_lastClickMs >= 0 && m_lastClickWindow == hit.windowId
        && m_clickClock.isValid()
        && static_cast<qreal>(m_clickClock.elapsed() - m_lastClickMs)
            <= m_doubleClickIntervalMs;
}

IconChipPointerDecision HybridIconChipRouter::pointerRelease(
    const HybridInput::PointerEvent &event)
{
    IconChipPointerDecision decision;
    updateHover(event.position, &decision);
    m_lastPosition = event.position;
    if (!m_pressed || event.changedButton != m_pressedButton) {
        return decision;
    }
    decision.consumed = true;
    const auto pressed = *m_pressed;
    const bool released_on_press_target = m_hovered == m_pressed;
    if (m_pressedButton == Qt::RightButton) {
        if (released_on_press_target) {
            decision.contextMenus.append({pressed.windowId, event.position});
        }
        resetPointer();
        return decision;
    }
    if (m_dragActive) {
        // AGENT-GUARD: click and drag completion are mutually exclusive; a
        // drag never also unrolls or closes on its release.
        appendDrag(HybridChrome::DragPhase::Commit, event.position, &decision);
    } else if (released_on_press_target && pressed.target == IconChipHitKind::Close) {
        decision.closeRequests.append(pressed.windowId);
        m_lastClickMs = -1;
    } else if (released_on_press_target && pressed.target == IconChipHitKind::Body) {
        if (isDoubleClick(pressed)) {
            decision.unrollRequests.append(pressed.windowId);
            m_lastClickMs = -1;
        } else {
            noteClick(pressed);
        }
    }
    resetPointer();
    return decision;
}

IconChipPointerDecision HybridIconChipRouter::pointerWheel(
    const QPointF &position, Qt::KeyboardModifiers modifiers, qreal awayFromUser)
{
    IconChipPointerDecision decision;
    if (m_pressed) {
        return decision;
    }
    updateHover(position, &decision);
    if (modifiers != Qt::NoModifier || qFuzzyIsNull(awayFromUser) || !m_hovered) {
        return decision;
    }
    decision.consumed = true;
    if (awayFromUser < 0.0) {
        decision.unrollRequests.append(m_hovered->windowId);
    }
    return decision;
}

IconChipPointerDecision HybridIconChipRouter::cancel()
{
    IconChipPointerDecision decision;
    cancelPointer(&decision);
    return decision;
}

IconChipPointerDecision HybridIconChipRouter::invalidateTargets()
{
    auto decision = cancel();
    if (m_hovered) {
        m_hovered.reset();
        decision.hoverChanged = true;
        decision.hovered.reset();
    }
    return decision;
}

} // namespace QindaQt::Compositor::KWinIntegration
