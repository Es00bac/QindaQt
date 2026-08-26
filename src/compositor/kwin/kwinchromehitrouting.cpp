// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinchromemanager.h"

#include "qindaqt/hybrid_chrome/chromehittest.h"
#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
std::optional<ChromePointerHit> KWinChromeManager::pointerHitAt(
    const QPointF &position) const
{
    for (auto iterator = m_stackingOrder.crbegin();
         iterator != m_stackingOrder.crend(); ++iterator) {
        if (m_contextQuarantine.contains(*iterator)) {
            continue;
        }
        const auto found = m_entries.find(*iterator);
        if (found == m_entries.end()) {
            continue;
        }
        if (!found->second.overlay->isVisible()) {
            // Hidden/minimized/focus-mode chrome has no visible affordance and
            // must not leave a global input region over the desktop.
            continue;
        }
        const auto target = HybridChrome::ChromeHitTester::hitTest(
            found->second.plan, position);
        if (target.isInteractive()
            || found->second.plan.outerFrame.contains(position)) {
            return ChromePointerHit{*iterator, target};
        }
    }
    return std::nullopt;
}

std::optional<ChromePointerHit> KWinChromeManager::pointerTargetAt(
    const QPointF &position) const
{
    auto hit = pointerHitAt(position);
    if (!hit) {
        return std::nullopt;
    }
    const auto found = m_entries.find(hit->containerId);
    if (found == m_entries.end()) {
        return std::nullopt;
    }
    for (const auto &member : found->second.plan.members) {
        if (member.titleDragRect.contains(position)) {
            // ChromeHitTester deliberately gives enlarged dividers precedence
            // for semantic target discovery. Ordinary compositor input has an
            // additional platform rule: native KDecoration owns every member
            // title pixel, including an overlapping divider or outer edge.
            hit->target = {HybridChrome::HitKind::MemberTitleDrag,
                           member.memberId, -1, std::nullopt, {}};
            break;
        }
    }
    return hit;
}

HybridInput::HitTarget KWinChromeManager::hitTestChrome(
    const QPointF &position) const
{
    const auto hit = pointerHitAt(position);
    if (!hit) {
        return {};
    }
    switch (hit->target.kind) {
    case HybridChrome::HitKind::OuterTitleDrag:
        return {HybridInput::HitKind::OuterTitle, hit->containerId, {}, {}};
    case HybridChrome::HitKind::MemberTitleDrag:
        return {HybridInput::HitKind::MemberTitle, hit->containerId,
                hit->target.stableId, {}};
    case HybridChrome::HitKind::Tab: {
        const auto &representatives =
            m_entries.at(hit->containerId).tabRepresentatives;
        const auto representative = representatives.value(hit->target.stableId);
        if (representative.isEmpty()) {
            return {};
        }
        auto target = HybridInput::HitTarget{HybridInput::HitKind::Tab,
                                             hit->containerId,
                                             representative,
                                             {}};
        target.pageId = hit->target.stableId;
        return target;
    }
    case HybridChrome::HitKind::Divider:
        return {HybridInput::HitKind::Divider, hit->containerId, {},
                hit->target.stableId};
    case HybridChrome::HitKind::None:
    case HybridChrome::HitKind::WindowButton:
    case HybridChrome::HitKind::OuterResize:
    case HybridChrome::HitKind::Client:
        return {};
    }
    return {};
}

std::optional<ChromeWindowActionRequest> KWinChromeManager::windowActionAt(
    const QPointF &position) const
{
    const auto hit = pointerHitAt(position);
    if (!hit || hit->target.kind != HybridChrome::HitKind::WindowButton
        || !hit->target.action) {
        return std::nullopt;
    }
    return ChromeWindowActionRequest{hit->containerId, *hit->target.action};
}

bool KWinChromeManager::requestWindowActionAt(const QPointF &position)
{
    const auto request = windowActionAt(position);
    if (!request) {
        return false;
    }
    Q_EMIT windowActionRequested(request->containerId, request->action);
    return true;
}

bool KWinChromeManager::dispatchPointerActivation(const ChromePointerHit &hit)
{
    if (m_contextQuarantine.contains(hit.containerId)) {
        return false;
    }
    const auto found = m_entries.find(hit.containerId);
    if (found == m_entries.end()) {
        return false;
    }
    if (hit.target.kind == HybridChrome::HitKind::WindowButton
        && hit.target.action) {
        const auto action = *hit.target.action;
        const auto &buttons = found->second.plan.buttons;
        if (std::none_of(buttons.cbegin(), buttons.cend(),
                         [action](const auto &button) {
                             return button.action == action;
                         })) {
            return false;
        }
        Q_EMIT windowActionRequested(hit.containerId, action);
        return true;
    }
    if (hit.target.kind == HybridChrome::HitKind::Tab
        && !hit.target.stableId.isEmpty()) {
        const auto &tabs = found->second.plan.tabs;
        const auto tab = std::find_if(tabs.cbegin(), tabs.cend(),
                                      [&hit](const auto &candidate) {
                                          return candidate.tabId
                                                  == hit.target.stableId
                                              && candidate.logicalIndex
                                                  == hit.target.logicalIndex;
                                      });
        if (tab == tabs.cend()
            || !found->second.tabRepresentatives.contains(hit.target.stableId)) {
            return false;
        }
        Q_EMIT tabActivationRequested(hit.containerId, hit.target.stableId);
        return true;
    }
    return false;
}

void KWinChromeManager::setPointerHover(std::optional<ChromePointerHit> hit)
{
    if (hit && (!hit->isValid() || !m_entries.contains(hit->containerId)
                || m_contextQuarantine.contains(hit->containerId))) {
        hit.reset();
    }
    m_pointerHover = std::move(hit);
    applyPointerHover();
}

void KWinChromeManager::applyPointerHover()
{
    for (auto &[containerId, entry] : m_entries) {
        entry.overlay->setPointerHoverTarget(
            m_pointerHover && m_pointerHover->containerId == containerId
                ? m_pointerHover->target
                : HybridChrome::ChromeHitTarget{});
    }
}

bool KWinChromeManager::setStackingAnchor(const QString &containerId,
                                          const QString &windowId,
                                          QString *error)
{
    const auto found = m_entries.find(containerId);
    if (found == m_entries.end() || windowId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("unknown chrome anchor '%1' for '%2'")
                         .arg(windowId, containerId);
        }
        return false;
    }
    const bool wasVisible = found->second.overlay->isVisible();
    const bool anchored = found->second.overlay->setStackingAnchor(windowId,
                                                                   error);
    const bool visible = found->second.overlay->isVisible();
    if (visible != wasVisible) {
        Q_EMIT overlayVisibilityChanged(containerId, visible);
    }
    return anchored;
}

void KWinChromeManager::setOverlayVisible(const QString &containerId,
                                           bool visible) noexcept
{
    const auto found = m_entries.find(containerId);
    if (found == m_entries.end()) {
        return;
    }
    const bool wasVisible = found->second.overlay->isVisible();
    if (visible && !m_contextQuarantine.contains(containerId)) {
        found->second.overlay->showOverlay();
    } else {
        found->second.overlay->hideOverlay();
        if (m_pointerHover && m_pointerHover->containerId == containerId) {
            m_pointerHover.reset();
            applyPointerHover();
        }
    }
    const bool isVisible = found->second.overlay->isVisible();
    if (isVisible != wasVisible) {
        Q_EMIT overlayVisibilityChanged(containerId, isVisible);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
