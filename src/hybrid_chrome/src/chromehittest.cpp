// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromehittest.h"

#include <cmath>

namespace QindaQt::HybridChrome {
namespace {

ChromeHitTarget resizeHit(const ChromeRenderPlan &plan, const QPointF &position)
{
    if (plan.maximized) {
        return {};
    }
    const auto margin = plan.metrics.outerResizeMargin;
    const auto expanded = plan.outerFrame.adjusted(-margin, -margin, margin, margin);
    if (!expanded.contains(position)) {
        return {};
    }
    Qt::Edges edges;
    if (std::abs(position.x() - plan.outerFrame.left()) <= margin) {
        edges |= Qt::LeftEdge;
    }
    if (std::abs(position.x() - plan.outerFrame.right()) <= margin) {
        edges |= Qt::RightEdge;
    }
    if (std::abs(position.y() - plan.outerFrame.top()) <= margin) {
        edges |= Qt::TopEdge;
    }
    if (std::abs(position.y() - plan.outerFrame.bottom()) <= margin) {
        edges |= Qt::BottomEdge;
    }
    return edges == Qt::Edges{}
        ? ChromeHitTarget{}
        : ChromeHitTarget{HitKind::OuterResize, plan.containerId, -1, std::nullopt, edges};
}

} // namespace

ChromeHitTarget ChromeHitTester::hitTest(const ChromeRenderPlan &plan,
                                         const QPointF &logicalPosition)
{
    for (const auto &button : plan.buttons) {
        if (button.rect.contains(logicalPosition)) {
            return {HitKind::WindowButton, plan.containerId, -1,
                    std::optional<WindowAction>(button.action), {}};
        }
    }
    if (const auto resize = resizeHit(plan, logicalPosition); resize.isInteractive()) {
        return resize;
    }
    if (!plan.outerFrame.contains(logicalPosition)) {
        return {};
    }
    for (const auto &tab : plan.tabs) {
        if (tab.rect.contains(logicalPosition)) {
            return {HitKind::Tab, tab.tabId, tab.logicalIndex, std::nullopt, {}};
        }
    }
    for (const auto &divider : plan.dividers) {
        if (divider.hitRect.contains(logicalPosition)) {
            return {HitKind::Divider, divider.dividerId, -1, std::nullopt, {}};
        }
    }
    for (const auto &member : plan.members) {
        if (member.titleDragRect.contains(logicalPosition)) {
            return {HitKind::MemberTitleDrag, member.memberId, -1, std::nullopt, {}};
        }
    }
    if (plan.outerTitleDragRect.contains(logicalPosition)) {
        return {HitKind::OuterTitleDrag, plan.containerId, -1, std::nullopt, {}};
    }
    if (plan.contentRect.contains(logicalPosition)) {
        return {HitKind::Client, {}, -1, std::nullopt, {}};
    }
    return {};
}

} // namespace QindaQt::HybridChrome
