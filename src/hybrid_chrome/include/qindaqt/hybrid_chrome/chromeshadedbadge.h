// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chrometypes.h"

#include <QFont>
#include <QFontMetricsF>

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
    // The label rect's bounds in logical pixels. The maximum exists so a
    // pathological title cannot make a rolled-up strip as wide as the screen;
    // it is not a target. ADR-0189 raised it from 140, which elided ordinary
    // page titles away.
    static constexpr qreal LabelMinimumWidth = 48.0;
    static constexpr qreal LabelMaximumWidth = 320.0;

    // The label a rolled-up badge shows: "<container name> · <foremost page>"
    // when the user named this container, the page title alone when they did
    // not, and the generated placeholder only when there is no page title
    // either (ADR-0168). Pure; the single source of truth for both the width
    // the strip reserves and the text paint() draws.
    [[nodiscard]] static QString resolveLabel(const QString &containerTitle,
                                              bool containerTitleIsGenerated,
                                              const QString &foremostTitle);

    // The font the badge label is painted with.
    //
    // AGENT-CONTRACT: paint() draws with the QPainter's font, and the
    // compositor paints chrome into a QImage whose painter carries the
    // application default font. Anything that changes the font the renderer
    // paints chrome with must change this too, or the strip will be sized for
    // one font and painted in another.
    [[nodiscard]] static QFont labelFont();

    // The label rect width to reserve for `label`: its measured advance plus
    // the label rect's own 4 px padding on each side, clamped to
    // [LabelMinimumWidth, LabelMaximumWidth]. An empty label still reserves
    // the minimum, so the badge keeps its shape.
    [[nodiscard]] static qreal labelWidthFor(const QString &label,
                                             const QFontMetricsF &metrics);

    // Transforms one engine-built shaded plan and its request tabs into badge
    // layout. Idempotent per plan; deterministic ordering.
    static void layout(ChromeRenderPlan *plan, const ChromeLayoutRequest &request);
    // Label, pills, and overflow counter. Controls and the 2 px identity
    // frame stay with the renderer's shared painting.
    static void paint(QPainter &painter, const ChromeRenderPlan &plan);
    // Content-driven outer width for a badge holding tabCount pills and a
    // label of labelWidth, used by the compositor's strip geometry to size and
    // anchor the shaded frame.
    //
    // AGENT-GUARD: labelWidth is required. It used to be a fixed
    // LabelMaximumWidth, which is why a long title could never widen the
    // strip: the strip was sized for 140 px of label whatever the title said,
    // and the label was then elided into it (ADR-0189).
    [[nodiscard]] static qreal badgeWidth(const ChromeMetrics &metrics,
                                          qsizetype tabCount, qreal labelWidth);
};

} // namespace QindaQt::HybridChrome
