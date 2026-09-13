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
    return plan;
}

} // namespace QindaQt::Compositor::KWinIntegration
