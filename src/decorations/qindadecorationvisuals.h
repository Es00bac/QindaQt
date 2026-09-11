// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QMarginsF>
#include <QRectF>

#include <memory>

class QPainter;

namespace KDecoration3 {
class DecorationShadow;
}

namespace QindaQt::Decoration {

struct DecorationVisualStyle {
    QColor frameColor;
    QColor shadowColor;
    qreal frameWidth = 1.0;
    qreal cornerRadius = 10.0;
    qreal shadowExtent = 12.0;
    qreal shadowOpacity = 0.30;
    bool framed = true;
};

[[nodiscard]] DecorationVisualStyle decorationVisualStyle(const QColor &border,
                                                          const QColor &surface,
                                                          bool maximized);
// Deterministic weathered Luna title bar: the authored paint color flakes
// away to a rust undercoat along the edges, with speckles and drips. The
// same seed always reproduces the same wear pattern, so repaints never
// flicker and every window weathers differently.
void paintWornLunaTitle(QPainter &painter, const QRectF &bar,
                        const QColor &paintColor, quint32 seed);
// Resize-only borders extend the interactive resize grip outside the frame.
// A grouped container member gets none: its frame is container-owned and
// native member resize is vetoed by the compositor (ADR-0117).
[[nodiscard]] QMarginsF decorationResizeOnlyBorders(bool maximized,
                                                    bool containerMember);
void paintDecorationFrame(QPainter &painter, const QRectF &bounds,
                          const DecorationVisualStyle &style);
[[nodiscard]] std::shared_ptr<KDecoration3::DecorationShadow>
createDecorationShadow(const DecorationVisualStyle &style);

} // namespace QindaQt::Decoration
