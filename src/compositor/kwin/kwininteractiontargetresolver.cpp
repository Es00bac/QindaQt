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
    ChromeExposureResolver chromeExposure)
    : m_registry(registry)
    , m_chrome(chrome)
    , m_chromeExposure(std::move(chromeExposure))
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
        const auto frame = window->frameGeometry();
        const auto client = window->clientGeometry();
        auto titleHeight = client.top() - frame.top();
        if (titleHeight < 8.0 || titleHeight > 96.0) {
            // CSD clients expose no server title metric. The explicit modifier
            // keeps this conservative fallback from stealing ordinary clicks.
            titleHeight = 30.0;
        }
        if (position.y() <= frame.top() + titleHeight && !id.isEmpty()) {
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
    const auto chromeHit = m_chrome
        ? m_chrome->hitTestChrome(position) : HybridInput::HitTarget{};
    const auto chromeTarget = tabDockTargetFromChromeHit(chromeHit);
    auto *window = topmostInputOwnerAt(position, source.memberId);
    HybridInput::DockTarget nativeTarget;
    QString nativeOwner;
    if (manageableNormalWindow(window)) {
        const auto id = m_registry.windowId(window);
        if (m_registry.window(id) == window) {
            nativeOwner = m_registry.owner(id);
            nativeTarget = targetFor(
                window, zoneAt(window->frameGeometry(), position));
        }
    }
    const bool sameContainer = chromeTarget.isValid() && !nativeOwner.isEmpty()
        && nativeOwner == chromeTarget.containerId;
    return dockTargetRespectingChromeExposure(
        chromeExposed(chromeHit, position, source.memberId), sameContainer,
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

KWin::Window *KWinInteractionTargetResolver::topmostInputOwnerAt(
    const QPointF &position, const QString &excludedWindowId) const
{
    const auto &stack = KWin::workspace()->stackingOrder();
    for (auto iterator = stack.crbegin(); iterator != stack.crend(); ++iterator) {
        auto *window = *iterator;
        if (!inputEligibleAt(window, position)) {
            continue;
        }
        const auto id = m_registry.windowId(window);
        if (!excludedWindowId.isEmpty() && id == excludedWindowId) {
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
    const QString &excludedWindowId) const
{
    if (!hit.isValid()) {
        return false;
    }
    return !m_chromeExposure
        || m_chromeExposure(hit.containerId, position, excludedWindowId);
}

KWin::Window *KWinInteractionTargetResolver::directionalWindow(
    KWin::Window *source, HybridInput::DockZone zone) const
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
