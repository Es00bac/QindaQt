// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwininteractiontargetresolver.h"

#include "hybriddocktargetrouting.h"
#include "managedwindowregistry.h"

#include <window.h>
#include <workspace.h>

#include <QtMath>

#include <limits>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool inputEligibleAt(const KWin::Window *window, const QPointF &position)
{
    return window && !window->isDeleted()
        && window->isOnCurrentActivity() && window->isOnCurrentDesktop()
        && !window->isMinimized() && !window->isHidden()
        && !window->isHiddenByShowDesktop() && window->readyForPainting()
        && window->hitTest(position);
}

bool manageableNormalWindow(const KWin::Window *window)
{
    return window && !window->isInternal() && !window->isPopupWindow()
        && !window->isTransient() && !window->isDialog()
        && window->isNormalWindow();
}

} // namespace

KWinInteractionTargetResolver::KWinInteractionTargetResolver(
    const ManagedWindowRegistry &registry,
    const ChromeHitProvider *chrome,
    ChromeExposureResolver chromeExposure,
    ContainerContentFrameResolver containerContentFrame,
    DraggedPageMembersResolver draggedPageMembers)
    : m_registry(registry)
    , m_chrome(chrome)
    , m_chromeExposure(std::move(chromeExposure))
    , m_containerContentFrame(std::move(containerContentFrame))
    , m_draggedPageMembers(std::move(draggedPageMembers))
{
}

HybridInput::HitTarget KWinInteractionTargetResolver::hitTest(
    const QPointF &position) const
{
    auto *window = topmostInputOwnerAt(position);
    HybridInput::HitTarget nativeTitle;
    QString nativeOwner;
    if (manageableNormalWindow(window)) {
        const auto id = m_registry.windowId(window);
        if (m_registry.window(id) != window) {
            window = nullptr;
        } else {
            nativeOwner = m_registry.owner(id);
        }
    } else {
        window = nullptr;
    }
    if (window) {
        const auto id = m_registry.windowId(window);
        // AGENT-GUARD: hitTest() backs only the exact Meta+Shift+Left grab
        // (InteractionController::pointerPress); the modifier chord already
        // disambiguates intent from an ordinary click, so any point over a
        // manageable window's own input region is a valid drag source, not
        // just its title strip. A narrower title-only strip left ordinary
        // client-area presses of the exact chord unconsumed, which fell
        // through to KWin's own decoration move and its Shift-drag
        // custom-tile default.
        if (!id.isEmpty()) {
            nativeTitle = {HybridInput::HitKind::MemberTitle,
                           m_registry.owner(id), id, {}};
        }
    }
    const auto chromeHit = m_chrome
        ? m_chrome->hitTestChrome(position) : HybridInput::HitTarget{};
    const bool sameContainer = chromeHit.isValid() && !nativeOwner.isEmpty()
        && nativeOwner == chromeHit.containerId;
    return sourceHitRespectingChromeExposure(
        chromeExposed(chromeHit, position), sameContainer,
        nativeTitle, chromeHit);
}

HybridInput::DockTarget KWinInteractionTargetResolver::pointerDockTarget(
    const HybridInput::HitTarget &source, const QPointF &position) const
{
    const auto exclusions = sourceExclusions(source);
    const auto chromeHit = m_chrome
        ? m_chrome->hitTestChrome(position) : HybridInput::HitTarget{};
    const auto chromeTarget = tabDockTargetFromChromeHit(chromeHit);
    auto *window = topmostInputOwnerAt(position, exclusions);
    HybridInput::DockTarget nativeTarget;
    QString nativeOwner;
    if (manageableNormalWindow(window)) {
        const auto id = m_registry.windowId(window);
        if (m_registry.window(id) == window) {
            nativeOwner = m_registry.owner(id);
            nativeTarget = targetFor(
                window, zoneAt(window->frameGeometry(), position));
            // AGENT-CONTRACT: A grouped member's tile is only part of its
            // container. Near the container's own edges the drop targets the
            // whole container (memberId empty) so the dropped window spans
            // the full container beside the existing layout;
            // HybridInteractionRuntime maps that to MoveAsRootSplit or
            // ReparentMemberToPageRoot. Deeper inside, the member-tile zones
            // above keep nested splits and per-member tab drops reachable.
            // Without this band every edge drop split the member tile nearest
            // the pointer, so a window could never be added beside a
            // multi-member layout.
            if (!nativeOwner.isEmpty() && m_containerContentFrame) {
                const auto contentFrame = m_containerContentFrame(nativeOwner);
                const auto edge = contentFrame
                    ? containerEdgeDockZone(*contentFrame, position)
                    : HybridInput::DockZone::None;
                if (edge != HybridInput::DockZone::None) {
                    nativeTarget = {nativeOwner, {}, edge};
                }
            }
        }
    }
    const bool sameContainer = chromeTarget.isValid() && !nativeOwner.isEmpty()
        && nativeOwner == chromeTarget.containerId;
    return dockTargetRespectingChromeExposure(
        chromeExposed(chromeHit, position, exclusions), sameContainer,
        chromeTarget, nativeTarget);
}

HybridInput::DockTarget KWinInteractionTargetResolver::keyboardDockTarget(
    const HybridInput::HitTarget &source, HybridInput::DockZone zone) const
{
    auto *sourceWindow = m_registry.window(source.memberId);
    if (!sourceWindow || zone == HybridInput::DockZone::None) {
        return {};
    }
    return targetFor(directionalWindow(sourceWindow, zone), zone);
}

HybridInput::DockTarget KWinInteractionTargetResolver::containerDirectionalTarget(
    const QString &containerId, const QString &sourceWindowId,
    HybridInput::DockZone zone) const
{
    auto *sourceWindow = m_registry.window(sourceWindowId);
    if (!sourceWindow || containerId.isEmpty() || zone == HybridInput::DockZone::None) {
        return {};
    }
    return targetFor(directionalWindow(sourceWindow, zone, containerId), zone);
}

KWin::Window *KWinInteractionTargetResolver::topmostInputOwnerAt(
    const QPointF &position, const QSet<QString> &excludedWindowIds) const
{
    const auto &stack = KWin::workspace()->stackingOrder();
    for (auto iterator = stack.crbegin(); iterator != stack.crend(); ++iterator) {
        auto *window = *iterator;
        if (!inputEligibleAt(window, position)) {
            continue;
        }
        const auto id = m_registry.windowId(window);
        if (excludedWindowIds.contains(id)) {
            continue;
        }
        // AGENT-CONTRACT: Stop at the first actual KWin input owner. If it is
        // not a manageable normal window, callers receive an invalid Hybrid
        // target instead of tunneling into a grouped window underneath.
        return window;
    }
    return nullptr;
}

bool KWinInteractionTargetResolver::chromeExposed(
    const HybridInput::HitTarget &hit,
    const QPointF &position,
    const QSet<QString> &excludedWindowIds) const
{
    if (!hit.isValid()) {
        return false;
    }
    return !m_chromeExposure
        || m_chromeExposure(hit.containerId, position, excludedWindowIds);
}

QSet<QString> KWinInteractionTargetResolver::sourceExclusions(
    const HybridInput::HitTarget &source) const
{
    QSet<QString> excluded;
    if (!source.memberId.isEmpty()) {
        excluded.insert(source.memberId);
    }
    // Only chrome tab drags carry a page identity (HybridChromeDragTranslator
    // sets it); a member drag must keep its own container's other tiles
    // reachable for within-container rearrangement.
    if (!source.pageId.isEmpty() && m_draggedPageMembers) {
        const auto members = m_draggedPageMembers(source.containerId, source.pageId);
        for (const auto &id : members) {
            if (!id.isEmpty()) {
                excluded.insert(id);
            }
        }
    }
    return excluded;
}

KWin::Window *KWinInteractionTargetResolver::directionalWindow(
    KWin::Window *source, HybridInput::DockZone zone,
    const QString &restrictToContainerId) const
{
    const auto sourceCenter = source->frameGeometry().center();
    KWin::Window *best = nullptr;
    auto bestScore = std::numeric_limits<qreal>::max();
    for (auto *candidate : KWin::workspace()->stackingOrder()) {
        if (!manageableNormalWindow(candidate) || !candidate->isShown()
            || !candidate->isOnCurrentActivity()
            || !candidate->isOnCurrentDesktop() || candidate == source
            || m_registry.windowId(candidate).isEmpty()) {
            continue;
        }
        if (!restrictToContainerId.isEmpty()
            && m_registry.owner(m_registry.windowId(candidate)) != restrictToContainerId) {
            continue;
        }
        const auto delta = candidate->frameGeometry().center() - sourceCenter;
        qreal primary = 0.0;
        qreal perpendicular = 0.0;
        switch (zone) {
        case HybridInput::DockZone::Left:
            primary = -delta.x();
            perpendicular = qAbs(delta.y());
            break;
        case HybridInput::DockZone::Right:
            primary = delta.x();
            perpendicular = qAbs(delta.y());
            break;
        case HybridInput::DockZone::Top:
            primary = -delta.y();
            perpendicular = qAbs(delta.x());
            break;
        case HybridInput::DockZone::Bottom:
            primary = delta.y();
            perpendicular = qAbs(delta.x());
            break;
        case HybridInput::DockZone::Tab:
            primary = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
            break;
        case HybridInput::DockZone::None:
            continue;
        }
        if (primary <= 0.0) {
            continue;
        }
        const auto score = primary + (perpendicular * 4.0);
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

HybridInput::DockTarget KWinInteractionTargetResolver::targetFor(
    KWin::Window *window, HybridInput::DockZone zone) const
{
    if (!window || zone == HybridInput::DockZone::None) {
        return {};
    }
    const auto id = m_registry.windowId(window);
    return {m_registry.owner(id), id, zone};
}

HybridInput::DockZone KWinInteractionTargetResolver::zoneAt(
    const QRectF &frame, const QPointF &position)
{
    if (!frame.isValid()) {
        return HybridInput::DockZone::None;
    }
    const auto x = (position.x() - frame.left()) / frame.width();
    const auto y = (position.y() - frame.top()) / frame.height();
    if (x >= 0.30 && x <= 0.70 && y >= 0.30 && y <= 0.70) {
        return HybridInput::DockZone::Tab;
    }
    const auto left = x;
    const auto right = 1.0 - x;
    const auto top = y;
    const auto bottom = 1.0 - y;
    const auto nearest = qMin(qMin(left, right), qMin(top, bottom));
    if (nearest == left) {
        return HybridInput::DockZone::Left;
    }
    if (nearest == right) {
        return HybridInput::DockZone::Right;
    }
    if (nearest == top) {
        return HybridInput::DockZone::Top;
    }
    return HybridInput::DockZone::Bottom;
}

} // namespace QindaQt::Compositor::KWinIntegration
