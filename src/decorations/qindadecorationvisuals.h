// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
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
void paintDecorationFrame(QPainter &painter, const QRectF &bounds,
                          const DecorationVisualStyle &style);
[[nodiscard]] std::shared_ptr<KDecoration3::DecorationShadow>
createDecorationShadow(const DecorationVisualStyle &style);

} // namespace QindaQt::Decoration
