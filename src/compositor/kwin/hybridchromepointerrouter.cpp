// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromepointerrouter.h"

#include <QLineF>

#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr qreal DefaultDragThreshold = 8.0;

} // namespace

bool ChromePointerHit::isValid() const noexcept
{
    return !containerId.isEmpty()
        && target.kind != HybridChrome::HitKind::None;
}

bool hasChromeDecisionOutput(const ChromePointerDecision &decision) noexcept
{
    return decision.hoverChanged
        || !decision.containerRaiseRequests.isEmpty()
        || !decision.activations.isEmpty() || !decision.drags.isEmpty()
        || !decision.contextMenus.isEmpty();
}

HybridChromePointerRouter::HybridChromePointerRouter(HitResolver resolver,
                                                     qreal dragThreshold)
    : m_resolver(std::move(resolver))
    , m_dragThreshold(dragThreshold)
{
    if (!std::isfinite(m_dragThreshold) || m_dragThreshold < 0.0) {
        m_dragThreshold = DefaultDragThreshold;
    }
}

std::optional<ChromePointerHit> HybridChromePointerRouter::hitAt(
    const QPointF &position) const
{
    if (!m_resolver) {
        return std::nullopt;
    }
    auto hit = m_resolver(position);
    return hit && hit->isValid() ? std::move(hit) : std::nullopt;
}

void HybridChromePointerRouter::updateHover(
    const QPointF &position, ChromePointerDecision *decision)
{
    const auto next = hitAt(position);
    if (next == m_hovered) {
        return;
    }
    m_hovered = next;
    decision->hoverChanged = true;
    decision->hovered = next;
}

bool HybridChromePointerRouter::ownsOrdinaryInput(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    using enum HybridChrome::HitKind;
    switch (target.kind) {
    case WindowButton:
    case Tab:
    case Divider:
    case OuterTitleDrag:
    case OuterResize:
        return true;
    case None:
    case MemberTitleDrag:
    case Client:
        // AGENT-CONTRACT: Returning false here is what hands application title
        // bars and content back to KWin's native KDecoration/client routing.
        return false;
    }
    return false;
}

bool HybridChromePointerRouter::isDragTarget(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    using enum HybridChrome::HitKind;
    switch (target.kind) {
    case Tab:
    case Divider:
    case OuterTitleDrag:
    case OuterResize:
        return true;
    case None:
    case WindowButton:
    case MemberTitleDrag:
    case Client:
        return false;
    }
    return false;
}

bool HybridChromePointerRouter::ownsContextMenuInput(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    // AGENT-CONTRACT: Only the synthetic outer title gets QindaQt's group
    // menu. Native member titles keep KWin's standard per-window menu.
    return target.kind == HybridChrome::HitKind::OuterTitleDrag;
}

bool HybridChromePointerRouter::isActivationTarget(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    return target.kind == HybridChrome::HitKind::WindowButton
        || target.kind == HybridChrome::HitKind::Tab;
}

void HybridChromePointerRouter::appendDrag(
    HybridChrome::DragPhase phase,
    const QPointF &position,
    ChromePointerDecision *decision) const
{
    if (!m_pressed) {
        return;
    }
    decision->drags.append({m_pressed->containerId,
                            {m_pressed->target, phase, position,
                             position - m_pressPosition}});
}

void HybridChromePointerRouter::resetPointer() noexcept
{
    m_pressed.reset();
    m_pressPosition = {};
    m_pressedButton = Qt::NoButton;
    m_dragActive = false;
}

void HybridChromePointerRouter::cancelPointer(ChromePointerDecision *decision)
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

ChromePointerDecision HybridChromePointerRouter::pointerMove(
    const HybridInput::PointerEvent &event)
{
    ChromePointerDecision decision;
    updateHover(event.position, &decision);
    const auto previousPosition = m_lastPosition;
    m_lastPosition = event.position;
    if (!m_pressed) {
        return decision;
    }

    decision.consumed = true;
    if (!event.buttons.testFlag(m_pressedButton)) {
        // AGENT-GUARD: A lost release must terminate compositor policy. Keeping
        // a stale grab would consume every later client motion indefinitely.
        cancelPointer(&decision);
        return decision;
    }
    if (m_pressedButton == Qt::RightButton) {
        return decision;
    }
    if (!isDragTarget(m_pressed->target)) {
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

ChromePointerDecision HybridChromePointerRouter::pointerPress(
    const HybridInput::PointerEvent &event)
{
    ChromePointerDecision decision;
    updateHover(event.position, &decision);
    m_lastPosition = event.position;
    if (event.changedButton != Qt::LeftButton
        && event.changedButton != Qt::RightButton) {
        return decision;
    }
    if (m_pressed) {
        cancelPointer(&decision);
    }
    if (event.modifiers != Qt::NoModifier
        || !event.buttons.testFlag(event.changedButton) || !m_hovered) {
        return decision;
    }
    const bool owned = event.changedButton == Qt::LeftButton
        ? ownsOrdinaryInput(m_hovered->target)
        : ownsContextMenuInput(m_hovered->target);
    if (!owned) {
        return decision;
    }

    m_pressed = m_hovered;
    m_pressPosition = event.position;
    m_lastPosition = event.position;
    m_pressedButton = event.changedButton;
    m_dragActive = false;
    decision.consumed = true;
    decision.containerRaiseRequests.append(m_pressed->containerId);
    return decision;
}

ChromePointerDecision HybridChromePointerRouter::pointerRelease(
    const HybridInput::PointerEvent &event)
{
    ChromePointerDecision decision;
    updateHover(event.position, &decision);
    m_lastPosition = event.position;
    if (!m_pressed || event.changedButton != m_pressedButton) {
        return decision;
    }

    decision.consumed = true;
    const auto pressed = *m_pressed;
    if (m_pressedButton == Qt::RightButton) {
        if (m_hovered == m_pressed) {
            decision.contextMenus.append(
                {pressed.containerId, event.position});
        }
        resetPointer();
        return decision;
    }
    if (m_dragActive) {
        appendDrag(HybridChrome::DragPhase::Commit, event.position, &decision);
    } else if (m_hovered == m_pressed && isActivationTarget(pressed.target)) {
        // AGENT-GUARD: Click and drag completion are mutually exclusive. A tab
        // drag must never reorder/detach and then activate on the same release.
        decision.activations.append(pressed);
    }
    resetPointer();
    return decision;
}

ChromePointerDecision HybridChromePointerRouter::cancel()
{
    ChromePointerDecision decision;
    cancelPointer(&decision);
    return decision;
}

ChromePointerDecision HybridChromePointerRouter::invalidateTargets()
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
