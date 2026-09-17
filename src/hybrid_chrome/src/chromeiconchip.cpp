// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromeiconchip.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::HybridChrome {
namespace {

bool reject(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

bool finitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

qreal clampedScale(qreal scale)
{
    if (!std::isfinite(scale)) {
        return 1.0;
    }
    return std::clamp(scale, ChromeIconChip::MinimumScale, ChromeIconChip::MaximumScale);
}

QRectF clampInto(QRectF frame, const QRectF &bounds)
{
    if (!bounds.isValid()) {
        return frame;
    }
    // A bounds rectangle narrower than the chip keeps the chip at its
    // leading edge rather than pushing it out of the far side.
    const qreal x = std::max(bounds.left(),
                             std::min(frame.left(), bounds.right() - frame.width()));
    const qreal y = std::max(bounds.top(),
                             std::min(frame.top(), bounds.bottom() - frame.height()));
    frame.moveTopLeft({x, y});
    return frame;
}

void paintPlaceholderGlyph(QPainter &painter, const IconChipPlan &plan)
{
    // No application icon: an identity-filled rounded square carrying a
    // miniature window outline, so the chip still reads as "a window".
    const qreal radius = 6.0 * plan.scale;
    painter.setPen(Qt::NoPen);
    painter.setBrush(plan.identity.handlebarFill);
    painter.drawRoundedRect(plan.iconRect, radius, radius);
    const QRectF window = plan.iconRect.adjusted(6.0 * plan.scale, 7.0 * plan.scale,
                                                 -6.0 * plan.scale, -6.0 * plan.scale);
    QPen pen(plan.identity.handlebarInk, std::max(1.0, 1.5 * plan.scale));
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(window, 2.0 * plan.scale, 2.0 * plan.scale);
    painter.drawLine(QLineF(window.left(), window.top() + 4.0 * plan.scale,
                            window.right(), window.top() + 4.0 * plan.scale));
}

} // namespace

bool IconChipPlan::isValid() const
{
    return !windowId.isEmpty() && frame.isValid() && imageRect.isValid()
        && std::isfinite(devicePixelRatio) && devicePixelRatio > 0.0
        && identity.isValid();
}

std::optional<IconChipPlan> ChromeIconChip::layout(const IconChipRequest &request,
                                                   QString *error)
{
    if (request.windowId.isEmpty()) {
        reject(error, QStringLiteral("icon chip request names no window"));
        return std::nullopt;
    }
    if (!finitePoint(request.anchor)) {
        reject(error, QStringLiteral("icon chip anchor is not finite"));
        return std::nullopt;
    }
    if (!std::isfinite(request.devicePixelRatio) || request.devicePixelRatio <= 0.0) {
        reject(error, QStringLiteral("icon chip device pixel ratio is invalid"));
        return std::nullopt;
    }
    QString paletteError;
    if (!request.palette.isValid(&paletteError)) {
        reject(error, QStringLiteral("icon chip palette is invalid: %1").arg(paletteError));
        return std::nullopt;
    }

    IconChipPlan plan;
    plan.windowId = request.windowId;
    plan.title = request.title;
    plan.devicePixelRatio = request.devicePixelRatio;
    plan.scale = clampedScale(request.scale);
    plan.palette = request.palette;
    plan.identity = resolveIdentityShades(request.identityColor, request.palette);
    plan.icon = request.icon;
    plan.focused = request.focused;

    const qreal extent = ChipExtent * plan.scale;
    plan.frame = clampInto(QRectF(request.anchor, QSizeF(extent, extent)), request.bounds);

    const qreal iconExtent = IconExtent * plan.scale;
    plan.iconRect = QRectF(plan.frame.center().x() - iconExtent / 2.0,
                           plan.frame.center().y() - iconExtent / 2.0,
                           iconExtent, iconExtent);

    // The close glyph straddles the pill's edge at the top-right 45° point,
    // half outside the circle so it never covers the icon.
    const qreal closeExtent = CloseExtent * plan.scale;
    const qreal radius = extent / 2.0;
    const QPointF closeCenter(plan.frame.center().x() + radius * M_SQRT1_2,
                              plan.frame.center().y() - radius * M_SQRT1_2);
    plan.closeRect = QRectF(closeCenter.x() - closeExtent / 2.0,
                            closeCenter.y() - closeExtent / 2.0,
                            closeExtent, closeExtent);

    plan.imageRect = plan.frame.united(plan.closeRect);
    if (!plan.title.isEmpty()) {
        const QFontMetricsF metrics((QFont()));
        const qreal inset = 8.0 * plan.scale;
        const qreal labelWidth = std::min(LabelMaximumWidth * plan.scale,
                                          metrics.horizontalAdvance(plan.title) + 2.0 * inset);
        const qreal labelHeight = metrics.height() + 6.0 * plan.scale;
        qreal labelX = plan.frame.right() + LabelGap * plan.scale;
        if (request.bounds.isValid() && labelX + labelWidth > request.bounds.right()) {
            labelX = plan.frame.left() - LabelGap * plan.scale - labelWidth;
        }
        plan.labelRect = QRectF(labelX, plan.frame.center().y() - labelHeight / 2.0,
                                labelWidth, labelHeight);
        plan.imageRect = plan.imageRect.united(plan.labelRect);
    }
    return plan;
}

IconChipHitKind ChromeIconChip::hitTest(const IconChipPlan &plan, const QPointF &position)
{
    if (!plan.isValid() || !finitePoint(position)) {
        return IconChipHitKind::None;
    }
    if (plan.closeRect.isValid()
        && QLineF(plan.closeRect.center(), position).length() <= plan.closeRect.width() / 2.0) {
        return IconChipHitKind::Close;
    }
    if (QLineF(plan.frame.center(), position).length() <= plan.frame.width() / 2.0) {
        return IconChipHitKind::Body;
    }
    return IconChipHitKind::None;
}

void ChromeIconChip::paint(QPainter &painter,
                           const IconChipPlan &plan,
                           const IconChipPaintState &state)
{
    if (!plan.isValid()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const bool emphasized = state.hovered || state.pressed || plan.focused;
    if (emphasized) {
        // Soft identity glow outside the pill, like the focused container ring.
        QPen glow(plan.identity.glow, 4.0 * plan.scale);
        painter.setPen(glow);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(plan.frame.adjusted(-1.0 * plan.scale, -1.0 * plan.scale,
                                                1.0 * plan.scale, 1.0 * plan.scale));
    }
    const qreal borderWidth = std::max(1.0, 1.5 * plan.scale);
    QPen border(emphasized ? plan.identity.border : plan.identity.borderDimmed, borderWidth);
    painter.setPen(border);
    painter.setBrush(state.pressed ? plan.palette.surface : plan.palette.surfaceRaised);
    painter.drawEllipse(plan.frame.adjusted(borderWidth / 2.0, borderWidth / 2.0,
                                            -borderWidth / 2.0, -borderWidth / 2.0));

    if (plan.icon.isNull()) {
        paintPlaceholderGlyph(painter, plan);
    } else {
        painter.drawImage(plan.iconRect, plan.icon);
    }

    if (state.hovered && plan.closeRect.isValid()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(state.closeHovered ? plan.palette.close.darker(115)
                                            : plan.palette.close);
        painter.drawEllipse(plan.closeRect);
        QPen cross(identityInk(plan.palette.close, plan.palette.text),
                   std::max(1.0, 1.5 * plan.scale), Qt::SolidLine, Qt::RoundCap);
        painter.setPen(cross);
        const QRectF glyph = plan.closeRect.adjusted(5.0 * plan.scale, 5.0 * plan.scale,
                                                     -5.0 * plan.scale, -5.0 * plan.scale);
        painter.drawLine(glyph.topLeft(), glyph.bottomRight());
        painter.drawLine(glyph.topRight(), glyph.bottomLeft());
    }

    if (state.hovered && plan.labelRect.isValid() && !plan.title.isEmpty()) {
        const qreal labelRadius = plan.labelRect.height() / 2.0;
        painter.setPen(QPen(plan.identity.border, std::max(1.0, 1.0 * plan.scale)));
        painter.setBrush(plan.palette.surfaceRaised);
        painter.drawRoundedRect(plan.labelRect, labelRadius, labelRadius);
        const QFontMetricsF metrics(painter.font());
        const qreal inset = 8.0 * plan.scale;
        const QRectF textRect = plan.labelRect.adjusted(inset, 0.0, -inset, 0.0);
        painter.setPen(plan.palette.text);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                         metrics.elidedText(plan.title, Qt::ElideRight,
                                            qRound(textRect.width())));
    }
    painter.restore();
}

} // namespace QindaQt::HybridChrome
