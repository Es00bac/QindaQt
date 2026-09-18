// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromepointerrouter.h"

#include <QElapsedTimer>
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
        || !decision.contextMenus.isEmpty() || !decision.shadeRequests.isEmpty();
}

HybridChromePointerRouter::HybridChromePointerRouter(HitResolver resolver,
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
        m_doubleClickIntervalMs = 400.0;
    }
    m_clickClock.start();
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

std::optional<HybridChromePointerRouter::TouchHit> HybridChromePointerRouter::hitNear(
    const QPointF &position, qreal radius, const std::optional<QRectF> &clip) const
{
    const auto owned = [this](const QPointF &probe) -> std::optional<TouchHit> {
        auto hit = hitAt(probe);
        if (hit && ownsOrdinaryInput(hit->target)) {
            return TouchHit{*hit, probe};
        }
        return std::nullopt;
    };
    // A finger that lands on something KWin owns (a member title bar, client
    // content) is KWin's: the ring never pulls it onto nearby chrome. Only a
    // finger on nothing at all is looked for nearby.
    if (const auto exact = hitAt(position)) {
        return ownsOrdinaryInput(exact->target) ? std::optional(TouchHit{*exact, position})
                                                : std::nullopt;
    }
    if (!std::isfinite(radius) || radius <= 0.0) {
        return std::nullopt;
    }
    // Eight compass probes at half the radius, then at the full radius: the
    // nearest ring wins, so a finger between two controls picks the closer.
    static constexpr double kDiagonal = 0.70710678118654752;
    const double offsets[8][2] = {{0.0, -1.0}, {0.0, 1.0}, {-1.0, 0.0}, {1.0, 0.0},
                                  {-kDiagonal, -kDiagonal}, {kDiagonal, -kDiagonal},
                                  {-kDiagonal, kDiagonal}, {kDiagonal, kDiagonal}};
    for (const double ring : {radius / 2.0, radius}) {
        for (const auto &offset : offsets) {
            const QPointF probe(position.x() + offset[0] * ring, position.y() + offset[1] * ring);
            if (clip && !clip->contains(probe)) {
                continue;
            }
            if (auto hit = owned(probe)) {
                return hit;
            }
        }
    }
    return std::nullopt;
}

bool HybridChromePointerRouter::rollTarget(const HybridChrome::ChromeHitTarget &target) noexcept
{
    return isRollTarget(target);
}

bool HybridChromePointerRouter::contextMenuTarget(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    return ownsContextMenuInput(target);
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
    case ContainerControl:
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
    case ContainerControl:
    case MemberTitleDrag:
    case Client:
        return false;
    }
    return false;
}

bool HybridChromePointerRouter::ownsContextMenuInput(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    // AGENT-CONTRACT: The container-aware menu owns right click on both
    // synthetic and native group headers. Left click on native member titles
    // still passes through to KDecoration for ordinary move behavior.
    return target.kind == HybridChrome::HitKind::OuterTitleDrag
        || target.kind == HybridChrome::HitKind::MemberTitleDrag
        || target.kind == HybridChrome::HitKind::Tab;
}

bool HybridChromePointerRouter::isActivationTarget(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    return target.kind == HybridChrome::HitKind::WindowButton
        || target.kind == HybridChrome::HitKind::ContainerControl
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

void HybridChromePointerRouter::noteBadgeClick(const ChromePointerHit &hit)
{
    m_lastBadgeClickMs = m_clickClock.isValid() ? m_clickClock.elapsed() : 0;
    m_lastBadgeClickContainer = hit.containerId;
}

bool HybridChromePointerRouter::isBadgeDoubleClick(const ChromePointerHit &hit) const
{
    return m_lastBadgeClickMs >= 0
        && m_lastBadgeClickContainer == hit.containerId
        && m_clickClock.isValid()
        && static_cast<qreal>(m_clickClock.elapsed() - m_lastBadgeClickMs)
            <= m_doubleClickIntervalMs;
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
        // ADR-0139: a pill click on a shaded badge unrolls to that tab. The
        // unroll request rides the same decision batch; the activation is
        // processed first and the final unshade reflow fixes member geometry.
        if (pressed.target.fromShadedBadge) {
            decision.shadeRequests.append({pressed.containerId, false});
        }
    } else if (m_hovered == m_pressed && !m_dragActive
               && pressed.target.kind == HybridChrome::HitKind::OuterTitleDrag
               && pressed.target.fromShadedBadge) {
        // A double-click anywhere on the badge body unrolls; a single click
        // only raises (already requested at press).
        if (isBadgeDoubleClick(pressed)) {
            decision.shadeRequests.append({pressed.containerId, false});
            m_lastBadgeClickMs = -1;
        } else {
            noteBadgeClick(pressed);
        }
    }
    resetPointer();
    return decision;
}

bool HybridChromePointerRouter::isRollTarget(
    const HybridChrome::ChromeHitTarget &target) noexcept
{
    return target.kind == HybridChrome::HitKind::OuterTitleDrag
        || target.kind == HybridChrome::HitKind::MemberTitleDrag
        || target.kind == HybridChrome::HitKind::Tab
        || target.kind == HybridChrome::HitKind::WindowButton
        || target.kind == HybridChrome::HitKind::ContainerControl;
}

ChromePointerDecision HybridChromePointerRouter::pointerWheel(
    const QPointF &position, Qt::KeyboardModifiers modifiers, qreal angleDelta)
{
    ChromePointerDecision decision;
    if (m_pressed) {
        return decision;
    }
    updateHover(position, &decision);
    if (modifiers != Qt::NoModifier || qFuzzyIsNull(angleDelta) || !m_hovered
        || !isRollTarget(m_hovered->target)) {
        return decision;
    }
    decision.consumed = true;
    decision.shadeRequests.append({m_hovered->containerId, angleDelta > 0.0});
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
