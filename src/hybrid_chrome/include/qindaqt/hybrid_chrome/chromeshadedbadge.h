// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chrometypes.h"

class QPainter;

namespace QindaQt::HybridChrome {

struct ChromeLayoutRequest;

// The rolled-up container badge (ADR-0139, ADR-0099's shaded strip). A
// shaded plan is badge-shaped: window buttons are dropped, the container
// controls collapse onto one edge, the label names the container and its
// foremost tab, and one tinted pill per tab (at most MaxPills, then a "+N"
// counter) unrolls to that tab. Layout is pure plan transformation; paint
// reads only the plan. The pill tabs are real ChromeRenderPlan::tabs
// entries, so hit testing and the accessibility adapter keep treating them
// as named tabs with no special case.
class ChromeShadedBadge final
{
public:
    static constexpr qsizetype MaxPills = 8;

    // Transforms one engine-built shaded plan and its request tabs into badge
    // layout. Idempotent per plan; deterministic ordering.
    static void layout(ChromeRenderPlan *plan, const ChromeLayoutRequest &request);
    // Label, pills, and overflow counter. Controls and the 2 px identity
    // frame stay with the renderer's shared painting.
    static void paint(QPainter &painter, const ChromeRenderPlan &plan);
    // Content-driven outer width for a badge holding tabCount pills, used by
    // the compositor's strip geometry to size and anchor the shaded frame.
    [[nodiscard]] static qreal badgeWidth(const ChromeMetrics &metrics,
                                          qsizetype tabCount);
};

} // namespace QindaQt::HybridChrome
