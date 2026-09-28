// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromehittest.h"

#include "qindaqt/hybrid_chrome/chromesplitdeck.h"

#include <cmath>

namespace QindaQt::HybridChrome {
namespace {

bool isSplitDeck(const ChromeRenderPlan &plan)
{
    return !plan.shaded && plan.style.titleLayout == ContainerTitleLayout::SplitDeck;
}

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
    // ADR-0281: a split-deck row's top edge exists only over its two painted
    // pieces. Above the gap there is no container, so no resize handle either;
    // the press must fall through like the rest of the gap.
    if (isSplitDeck(plan) && edges.testFlag(Qt::TopEdge)) {
        const auto over = [&](const QRectF &piece) {
            return piece.isValid() && position.x() >= piece.left() - margin
                && position.x() <= piece.right() + margin;
        };
        if (!over(plan.titleTab) && !over(plan.deckPiece)) {
            edges &= ~Qt::Edges(Qt::TopEdge);
        }
    }
    if (std::abs(position.y() - plan.outerFrame.bottom()) <= margin) {
        edges |= Qt::BottomEdge;
    }
    return edges == Qt::Edges{}
        ? ChromeHitTarget{}
        : ChromeHitTarget{HitKind::OuterResize, plan.containerId, -1,
                          std::nullopt, edges, std::nullopt};
}

} // namespace

ChromeHitTarget ChromeHitTester::hitTest(const ChromeRenderPlan &plan,
                                         const QPointF &logicalPosition)
{
    for (const auto &button : plan.buttons) {
        if (button.rect.contains(logicalPosition)) {
            return {HitKind::WindowButton, plan.containerId, -1,
                    std::optional<WindowAction>(button.action), {}, std::nullopt};
        }
    }
    for (const auto &control : plan.controls) {
        if (control.rect.contains(logicalPosition)) {
            return {HitKind::ContainerControl, plan.containerId, -1,
                    std::nullopt, {}, control.control};
        }
    }
    // AGENT-CONTRACT: The rolled-up badge offers no resize affordance: its
    // frame never resizes (shade freezes the committed layout, ADR-0099).
    if (!plan.shaded) {
        if (const auto resize = resizeHit(plan, logicalPosition); resize.isInteractive()) {
            return resize;
        }
    }
    if (!plan.outerFrame.contains(logicalPosition)) {
        return {};
    }
    const bool splitDeck = isSplitDeck(plan);
    if (splitDeck) {
        // Carousel cards overlap; the nearest card owns the point.
        if (const auto index = ChromeSplitDeck::tabAt(plan, logicalPosition)) {
            const auto &tab = plan.tabs[*index];
            return {HitKind::Tab, tab.tabId, tab.logicalIndex, std::nullopt, {},
                    std::nullopt, false, true};
        }
    } else {
        for (const auto &tab : plan.tabs) {
            if (tab.rect.contains(logicalPosition)) {
                return {HitKind::Tab, tab.tabId, tab.logicalIndex, std::nullopt, {},
                        std::nullopt, plan.shaded};
            }
        }
    }
    for (const auto &divider : plan.dividers) {
        if (divider.hitRect.contains(logicalPosition)) {
            return {HitKind::Divider, divider.dividerId, -1, std::nullopt, {},
                    std::nullopt};
        }
    }
    for (const auto &member : plan.members) {
        if (member.titleDragRect.contains(logicalPosition)) {
            return {HitKind::MemberTitleDrag, member.memberId, -1, std::nullopt, {},
                    std::nullopt};
        }
    }
    if (plan.outerTitleDragRect.contains(logicalPosition)) {
        return {HitKind::OuterTitleDrag, plan.containerId, -1, std::nullopt, {},
                std::nullopt, plan.shaded};
    }
    // The deck's own surface around its cards moves the container too, and a
    // wheel over it steps tabs like a wheel over a card. The gap between the
    // two pieces matches nothing below, so the press is not chrome's.
    if (splitDeck && plan.deckPiece.contains(logicalPosition)
        && logicalPosition.y() < plan.outerTitleBar.bottom()) {
        return {HitKind::OuterTitleDrag, plan.containerId, -1, std::nullopt, {},
                std::nullopt, false, true};
    }
    if (plan.contentRect.contains(logicalPosition)) {
        return {HitKind::Client, {}, -1, std::nullopt, {}, std::nullopt};
    }
    return {};
}

} // namespace QindaQt::HybridChrome
