// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindadecorationvisuals.h"

#include <KDecoration3/DecorationShadow>

#include <QImage>
#include <QMarginsF>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace QindaQt::Decoration {
namespace {

QColor inkShadow(const QColor &surface)
{
    const QColor source = surface.isValid() ? surface : QColor(Qt::black);
    return QColor::fromRgb(source.red() / 4, source.green() / 4,
                           source.blue() / 4);
}

qreal roundedRectDistance(const QPointF &point, const QRectF &box, qreal radius)
{
    const QPointF delta(
        std::abs(point.x() - box.center().x()) - box.width() / 2.0 + radius,
        std::abs(point.y() - box.center().y()) - box.height() / 2.0 + radius);
    const QPointF outside(std::max(delta.x(), 0.0), std::max(delta.y(), 0.0));
    return std::hypot(outside.x(), outside.y()) +
           std::min(std::max(delta.x(), delta.y()), 0.0) - radius;
}

} // namespace

DecorationVisualStyle decorationVisualStyle(const QColor &border,
                                            const QColor &surface,
                                            bool maximized)
{
    DecorationVisualStyle style;
    style.frameColor = border;
    style.shadowColor = inkShadow(surface);
    style.framed = !maximized;
    return style;
}

QMarginsF decorationResizeOnlyBorders(bool maximized, bool containerMember)
{
    return maximized || containerMember ? QMarginsF{}
                                        : QMarginsF(5.0, 5.0, 5.0, 5.0);
}

void paintDecorationFrame(QPainter &painter, const QRectF &bounds,
                          const DecorationVisualStyle &style)
{
    if (!style.framed || !style.frameColor.isValid() || bounds.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(style.frameColor, style.frameWidth));
    const qreal inset = style.frameWidth / 2.0;
    const QRectF frame = bounds.adjusted(inset, inset, -inset, -inset);
    const qreal radius = std::max(0.0, style.cornerRadius - inset);
    painter.drawRoundedRect(frame, radius, radius);
    painter.restore();
}

std::shared_ptr<KDecoration3::DecorationShadow>
createDecorationShadow(const DecorationVisualStyle &style)
{
    if (!style.framed || !style.shadowColor.isValid() ||
        style.shadowExtent <= 0.0 || style.shadowOpacity <= 0.0) {
        return {};
    }

    const int extent = std::max(1, qCeil(style.shadowExtent));
    const int radius = std::max(0, qCeil(style.cornerRadius));
    const int boxSpan = radius * 2 + 1;
    QImage texture(QSize(boxSpan + extent * 2, boxSpan + extent * 2),
                   QImage::Format_ARGB32_Premultiplied);
    texture.fill(Qt::transparent);
    const QRectF box(extent, extent, boxSpan, boxSpan);

    // AGENT-GUARD: The texture center is the KDecoration nine-patch stretch
    // cell. Keep the shadow outside the sample box so stretching never paints
    // over client content.
    for (int y = 0; y < texture.height(); ++y) {
        for (int x = 0; x < texture.width(); ++x) {
            const qreal distance = roundedRectDistance(
                QPointF(x + 0.5, y + 0.5), box, style.cornerRadius);
            if (distance <= 0.0 || distance >= style.shadowExtent) {
                continue;
            }
            const qreal progress = 1.0 - distance / style.shadowExtent;
            const qreal falloff = progress * progress * (3.0 - 2.0 * progress);
            QColor pixel = style.shadowColor;
            pixel.setAlpha(qRound(
                255.0 * std::clamp(style.shadowOpacity * falloff, 0.0, 1.0)));
            texture.setPixelColor(x, y, pixel);
        }
    }

    auto shadow = std::make_shared<KDecoration3::DecorationShadow>();
    shadow->setPadding(QMarginsF(extent, extent, extent, extent));
    shadow->setInnerShadowRect(
        QRectF(texture.rect().center(), QSizeF(1.0, 1.0)));
    shadow->setShadow(texture);
    return shadow;
}

} // namespace QindaQt::Decoration
