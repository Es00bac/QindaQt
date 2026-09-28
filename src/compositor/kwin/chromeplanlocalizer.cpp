// SPDX-License-Identifier: GPL-3.0-or-later
#include "chromeplanlocalizer.h"

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void translateRect(QRectF *rect, const QPointF &offset)
{
    rect->translate(offset);
}

} // namespace

HybridChrome::ChromeRenderPlan localizeChromeRenderPlan(
    HybridChrome::ChromeRenderPlan plan, const QPointF &origin)
{
    const QPointF offset = -origin;
    translateRect(&plan.outerFrame, offset);
    translateRect(&plan.outerTitleBar, offset);
    translateRect(&plan.outerTitleDragRect, offset);
    translateRect(&plan.tabStrip, offset);
    translateRect(&plan.contentRect, offset);
    for (auto &button : plan.buttons) {
        translateRect(&button.rect, offset);
    }
    for (auto &control : plan.controls) {
        translateRect(&control.rect, offset);
    }
    for (auto &tab : plan.tabs) {
        translateRect(&tab.rect, offset);
    }
    for (auto &member : plan.members) {
        translateRect(&member.windowRect, offset);
        translateRect(&member.titleDragRect, offset);
    }
    for (auto &divider : plan.dividers) {
        translateRect(&divider.visualRect, offset);
        translateRect(&divider.hitRect, offset);
    }
    // AGENT-GUARD: the rolled-up badge's own rectangles. They were missed when
    // the badge was added (ADR-0139), so the badge label and the keyboard
    // selection chip were painted at *global* coordinates inside a
    // frame-local image and clipped away entirely — a rolled-up container
    // showed controls and pills but never its title, which is the defect
    // ADR-0163 and ADR-0168 both tried to fix by changing the label *text*
    // while nothing was being drawn at all. Every painted rectangle on
    // ChromeRenderPlan must appear in this function; a new one that does not
    // is invisible rather than misplaced, which is why it can go unnoticed.
    translateRect(&plan.badgeLabelRect, offset);
    translateRect(&plan.indexBadgeRect, offset);
    // The split-deck pieces (ADR-0281) repeated that miss: the name tab, the
    // deck piece and the name were painted at global coordinates, so a Corner
    // Bar container's title row only lined up with its windows at the
    // screen's top-left corner (owner, 2026-09-28).
    translateRect(&plan.titleTab, offset);
    translateRect(&plan.deckPiece, offset);
    translateRect(&plan.titleLabelRect, offset);
    return plan;
}

} // namespace QindaQt::Compositor::KWinIntegration
