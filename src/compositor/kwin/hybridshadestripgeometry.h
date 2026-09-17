// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QRect>

#include <QtTypes>

namespace QindaQt::Compositor::KWinIntegration {

// Width and anchoring for the rolled-up container's strip frame (ADR-0139,
// ADR-0099). The badge is anchored at the container frame's left edge: same
// top-left, content-driven width, so unrolling keeps restoring the exact
// original size at the strip's current position. Pure values; no KWin state.
class HybridShadeStripGeometry final
{
public:
    // Outer strip width (badge content width plus its border) for a
    // container with tabCount pages and a badge label of labelWidth logical
    // pixels, never wider than the container frame it anchors to and never
    // smaller than one logical pixel.
    //
    // AGENT-GUARD: labelWidth is what makes a rolled-up strip grow with its
    // title (ADR-0189). Measure it with
    // ChromeShadedBadge::labelWidthFor(ChromeShadedBadge::resolveLabel(...))
    // so the reserved width and the painted text agree; passing a constant
    // reintroduces the defect where a long title was elided into a fixed
    // 140 px label.
    [[nodiscard]] static int stripWidth(qsizetype tabCount,
                                        const QRect &containerFrame,
                                        qreal labelWidth);
};

} // namespace QindaQt::Compositor::KWinIntegration
