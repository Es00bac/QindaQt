// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRandomGenerator>

#include <cmath>

// The worn Luna title texture (ADR-0124): rust undercoat, surviving paint,
// edge chips, speckles and weather streaks from a deterministic seed. Moved
// out of decoration_painter.cpp unchanged for size (ADR-0264); it is still
// part of the one chrome renderer (ADR-0127).
namespace QindaQt::Decoration {
namespace {

constexpr qreal kPi = 3.14159265358979323846;

QPointF onBar(const QRectF &bar, QRandomGenerator &generator)
{
    return QPointF(bar.left() + generator.bounded(int(bar.width())),
                   bar.top() + generator.bounded(int(bar.height())));
}

} // namespace

void paintWornLunaTitle(QPainter &painter, const QRectF &bar,
                        const QColor &paintColor, quint32 seed)
{
    if (!paintColor.isValid() || bar.isEmpty()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = DecorationCornerRadius;

    // Rust undercoat: what the Luna paint flaked away from.
    QLinearGradient undercoat(bar.topLeft(), bar.bottomLeft());
    undercoat.setColorAt(0.0, QColor(QStringLiteral("#4a2f1b")));
    undercoat.setColorAt(1.0, QColor(QStringLiteral("#6f4526")));
    QPainterPath underPath;
    underPath.addRoundedRect(bar, radius, radius);
    painter.fillPath(underPath, undercoat);

    // Surviving Luna paint keeps its original vertical sheen.
    QLinearGradient sheen(bar.topLeft(), bar.bottomLeft());
    sheen.setColorAt(0.0, paintColor.lighter(122));
    sheen.setColorAt(0.55, paintColor);
    sheen.setColorAt(1.0, paintColor.darker(126));
    QPainterPath paintPath;
    paintPath.addRoundedRect(bar, radius, radius);
    painter.fillPath(paintPath, sheen);

    // Thin surviving gloss line under the top edge.
    painter.fillRect(QRectF(bar.left(), bar.top() + 1.0, bar.width(), 1.2),
                     QColor(255, 255, 255, 70));

    QRandomGenerator generator(seed);
    painter.setPen(Qt::NoPen);

    // Edge chips: paint flaked off along the top and bottom seams. Every
    // chip reveals the undercoat with a slightly darker worn rim.
    const QColor chipFill(QStringLiteral("#5f3d22"));
    const QColor chipRim(QStringLiteral("#3e2716"));
    const int chipCount = qBound(4, qRound(bar.width() / 95.0), 24);
    for (int i = 0; i < chipCount; ++i) {
        const bool topEdge = generator.bounded(2) == 0;
        const qreal cx = bar.left() + generator.bounded(int(bar.width()));
        const qreal cy = topEdge ? bar.top() + generator.bounded(3)
                                 : bar.bottom() - 1.0 - generator.bounded(3);
        const qreal span = 1.8 + generator.bounded(40) / 10.0;
        QPolygonF chip;
        const int points = 3 + generator.bounded(3);
        for (int p = 0; p < points; ++p) {
            const qreal angle = (p / qreal(points)) * 2.0 * kPi
                + generator.bounded(628) / 100.0;
            const qreal extent = span * (0.6 + generator.bounded(100) / 160.0);
            chip.append(QPointF(cx + std::cos(angle) * extent,
                                cy + std::sin(angle) * extent * 0.7));
        }
        painter.setBrush(chipFill);
        painter.drawPolygon(chip);
        painter.setBrush(Qt::NoBrush);
        QColor rimColor = chipRim;
        rimColor.setAlpha(150);
        QPen rim(rimColor, 0.6);
        painter.setPen(rim);
        painter.drawPolygon(chip);
        painter.setPen(Qt::NoPen);
    }

    // Rust speckles bloom through the paint, denser toward the bottom seam.
    const int speckles = qBound(6, qRound(bar.width() / 42.0), 60);
    for (int i = 0; i < speckles; ++i) {
        const QPointF spot = onBar(bar, generator);
        QColor rust(QStringLiteral("#8c5427"));
        rust.setAlpha(90 + generator.bounded(110));
        painter.setBrush(rust);
        const qreal extent = 0.6 + generator.bounded(90) / 90.0;
        painter.drawEllipse(spot, extent, extent);
    }

    // Weather streaks run from the bottom seam downward and fade out.
    const int drips = qBound(2, qRound(bar.width() / 240.0), 8);
    for (int i = 0; i < drips; ++i) {
        const qreal cx = bar.left() + 8.0
            + generator.bounded(qMax(1, int(bar.width()) - 16));
        const qreal length = 3.0 + generator.bounded(90) / 10.0;
        QLinearGradient drip(cx, qMax(bar.top(), bar.bottom() - length), cx,
                             bar.bottom());
        QColor stain(QStringLiteral("#74451f"));
        QColor faded = stain;
        faded.setAlpha(0);
        drip.setColorAt(0.0, faded);
        drip.setColorAt(1.0, stain);
        painter.setBrush(drip);
        painter.drawRect(QRectF(cx, bar.bottom() - length, 1.1, length));
    }
    painter.restore();
}

} // namespace QindaQt::Decoration
