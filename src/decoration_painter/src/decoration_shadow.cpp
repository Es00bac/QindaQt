// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_shadow.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace QindaQt::Decoration {
namespace {

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

DecorationShadowTexture
decorationShadowTexture(const DecorationVisualStyle &style)
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

    return {texture, QMarginsF(extent, extent, extent, extent),
            QRectF(texture.rect().center(), QSizeF(1.0, 1.0))};
}

DecorationShadowTexture
decorationShadowTexture(const DecorationVisualStyle &style,
                       const DecorationChrome &chrome,
                       const DecorationFrameVisual &frame)
{
    const qreal tabWidth = decorationTitleTabWidth(chrome, frame);
    if (frame.memberHandle || tabWidth >= frame.size.width()) {
        return decorationShadowTexture(style);
    }
    if (!style.framed || !style.shadowColor.isValid() ||
        style.shadowExtent <= 0.0 || style.shadowOpacity <= 0.0 || frame.size.isEmpty()) {
        return {};
    }

    const int extent = std::max(1, qCeil(style.shadowExtent));
    const qreal titleHeight = decorationTitleHeight(chrome);
    const qreal radius = std::max(0.0, style.cornerRadius);
    const int width = qCeil(frame.size.width());
    // AGENT-GUARD: Preserve the whole horizontal silhouette in the fixed
    // corner cells. Stretch only one opaque-body row vertically: a generic
    // rectangular nine-patch invents a shadow above the transparent cutout.
    const int height = std::max(1, std::min(qCeil(frame.size.height()),
                                          qCeil(titleHeight + 2.0 * radius + 1.0)));
    QImage texture(QSize(width + 2 * extent, height + 2 * extent),
                   QImage::Format_ARGB32_Premultiplied);
    texture.fill(Qt::transparent);
    const QRectF tab(0.0, 0.0, tabWidth, titleHeight + radius);
    const QRectF body(0.0, titleHeight, width, std::max(0.0, height - titleHeight));
    const QRectF shoulder(0.0, titleHeight, width, radius);
    for (int y = 0; y < texture.height(); ++y) {
        for (int x = 0; x < texture.width(); ++x) {
            const QPointF point(x + 0.5 - extent, y + 0.5 - extent);
            // Same tab/body/shoulder union as paintTabFrame. No interior
            // shadow is emitted, including where the tab joins the body.
            const qreal distance = std::min({
                roundedRectDistance(point, tab, radius),
                roundedRectDistance(point, body, radius),
                roundedRectDistance(point, shoulder, 0.0)});
            if (distance <= 0.0 || distance >= style.shadowExtent) {
                continue;
            }
            const qreal progress = 1.0 - distance / style.shadowExtent;
            const qreal falloff = progress * progress * (3.0 - 2.0 * progress);
            QColor pixel = style.shadowColor;
            pixel.setAlpha(qRound(255.0 * std::clamp(style.shadowOpacity * falloff, 0.0, 1.0)));
            texture.setPixelColor(x, y, pixel);
        }
    }

    return {texture, QMarginsF(extent, extent, extent, extent),
            QRectF(extent + width / 2,
                extent + std::clamp(qCeil(titleHeight + radius), 0, height - 1), 1.0, 1.0)};
}

void paintDecorationShadow(QPainter &painter, const DecorationVisualStyle &style,
                           const DecorationChrome &chrome, const DecorationFrameVisual &frame)
{
    const auto shadow = decorationShadowTexture(style, chrome, frame);
    if (shadow.image.isNull()) return;
    const qreal sx[] = {0.0, shadow.innerRect.left(), shadow.innerRect.right(),
                        qreal(shadow.image.width())};
    const qreal sy[] = {0.0, shadow.innerRect.top(), shadow.innerRect.bottom(),
                        qreal(shadow.image.height())};
    const qreal dx[] = {-shadow.padding.left(), sx[1] - shadow.padding.left(),
        frame.size.width() + shadow.padding.right() - (sx[3] - sx[2]),
        frame.size.width() + shadow.padding.right()};
    const qreal dy[] = {-shadow.padding.top(), sy[1] - shadow.padding.top(),
        frame.size.height() + shadow.padding.bottom() - (sy[3] - sy[2]),
        frame.size.height() + shadow.padding.bottom()};
    // AGENT-CONTRACT: Same eight fixed-corner/stretch-edge cells consumed by
    // KDecoration/KWin. The center is never drawn over client content. This
    // keeps previews on the exact production texture instead of a simulated
    // rectangular drop shadow (ADR-0127).
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (row == 1 && column == 1) continue;
            const QRectF target(dx[column], dy[row], dx[column + 1] - dx[column],
                                dy[row + 1] - dy[row]);
            if (target.isEmpty()) continue;
            painter.drawImage(target, shadow.image,
                QRectF(sx[column], sy[row], sx[column + 1] - sx[column],
                       sy[row + 1] - sy[row]));
        }
    }
    painter.restore();
}

} // namespace QindaQt::Decoration
