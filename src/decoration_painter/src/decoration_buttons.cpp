// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

#include <algorithm>

// Window decoration buttons: arrangement and the three button renderers
// (classic lights, worn Luna glyphs, flat symbols). Split from the title and
// caption painter only for size; both files are one renderer (ADR-0127).
namespace QindaQt::Decoration {
namespace {

constexpr qreal kLeftClusterInset = 12.0;
constexpr qreal kRightClusterInset = 14.0;

void paintClassicGlyph(QPainter &painter, const DecorationChrome &chrome,
                       const DecorationFrameVisual &frame,
                       const DecorationButtonVisual &button, const QRectF &circle)
{
    QPen pen(decorationButtonGlyphColor(chrome, button.kind, frame.active));
    pen.setWidthF(1.15);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = circle.center();
    const qreal radius = circle.width() * 0.20;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        painter.drawLine(center + QPointF(-radius, -radius),
                         center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius),
                         center + QPointF(-radius, radius));
        break;
    case DecorationButtonKind::Minimize:
        painter.drawLine(center + QPointF(-radius, radius * 0.45),
                         center + QPointF(radius, radius * 0.45));
        break;
    case DecorationButtonKind::Maximize:
        if (frame.restoreGlyph) {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius * 0.55,
                                    radius * 1.45, radius * 1.45));
            painter.drawRect(QRectF(center.x() - radius * 0.45, center.y() - radius,
                                    radius * 1.45, radius * 1.45));
        } else {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2.0, radius * 2.0));
        }
        break;
    case DecorationButtonKind::More:
        break;
    }
}

void paintGlyphChrome(QPainter &painter, const DecorationChrome &chrome,
                      const DecorationFrameVisual &frame,
                      const DecorationButtonVisual &button, const QRectF &circle)
{
    // AGENT-NOTE: glyph chrome draws the console-style outline glyphs in their
    // own colors directly on the Luna paint. The dash-pattern stroke reads as
    // chipped paint at 16 px; the shapes stay recognizable when eroded.
    QColor stroke = decorationGlyphChromeColor(chrome, button.kind, frame.restoreGlyph);
    if (!frame.active) {
        auto dimmed = stroke.toHsl();
        dimmed.setHslF(dimmed.hslHueF(),
                       dimmed.saturationF() * 0.45f,
                       qBound(0.0f, static_cast<float>(dimmed.lightnessF() * 0.9), 1.0f),
                       stroke.alphaF());
        stroke = dimmed;
    }
    if (button.pressed) {
        stroke = stroke.darker(130);
    } else if (button.hovered) {
        stroke = stroke.lighter(115);
    }

    if (button.hovered || button.pressed) {
        const QColor halo = button.pressed ? QColor(0, 0, 0, 60)
                                           : QColor(255, 255, 255, 46);
        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawRoundedRect(circle.adjusted(-1.0, -1.0, -1.0, -1.0), 3.5, 3.5);
    }

    QPen pen(QBrush(stroke), 1.8);
    pen.setDashPattern({5.0, 1.6, 3.0, 1.2});
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = circle.center();
    const qreal radius = circle.width() * 0.30;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        painter.drawLine(center + QPointF(-radius, -radius),
                         center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius),
                         center + QPointF(-radius, radius));
        break;
    case DecorationButtonKind::Minimize:
        painter.drawEllipse(center, radius, radius);
        break;
    case DecorationButtonKind::Maximize:
        if (frame.restoreGlyph) {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2.0, radius * 2.0));
        } else {
            QPainterPath triangle;
            triangle.moveTo(center + QPointF(0.0, -radius));
            triangle.lineTo(center + QPointF(radius * 1.1, radius * 0.8));
            triangle.lineTo(center + QPointF(-radius * 1.1, radius * 0.8));
            triangle.closeSubpath();
            painter.drawPath(triangle);
        }
        break;
    case DecorationButtonKind::More:
        break;
    }
}

void paintFlatButton(QPainter &painter, const DecorationChrome &chrome,
                     const DecorationFrameVisual &frame,
                     const DecorationButtonVisual &button)
{
    // Flat symbol buttons (ADR-0129): monochrome glyphs in the caption ink,
    // always visible, with a soft hover plate. Close takes its own color on
    // hover. Caption ink keeps the style legible on classic and worn Luna
    // title bars alike.
    const QRectF box = button.geometry;
    QColor ink = decorationCaptionColor(chrome, frame.active);
    if (!frame.active) {
        ink.setAlphaF(0.72f);
    }
    if (button.hovered || button.pressed) {
        QColor plate;
        if (button.kind == DecorationButtonKind::Close && chrome.close.isValid()) {
            plate = button.pressed ? chrome.close.darker(118) : chrome.close;
            ink = qGray(plate.rgb()) < 150 ? QColor(Qt::white) : QColor(Qt::black);
        } else {
            plate = decorationCaptionColor(chrome, frame.active);
            plate.setAlpha(button.pressed ? 72 : 40);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(plate);
        painter.drawRoundedRect(box, 3.0, 3.0);
    }
    QPen pen(ink, 1.3);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QPointF center = box.center();
    const qreal radius = box.width() * 0.24;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        painter.drawLine(center + QPointF(-radius, -radius),
                         center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius),
                         center + QPointF(-radius, radius));
        break;
    case DecorationButtonKind::Minimize:
        painter.drawLine(center + QPointF(-radius, 0.0), center + QPointF(radius, 0.0));
        break;
    case DecorationButtonKind::Maximize:
        if (frame.restoreGlyph) {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius * 0.5,
                                    radius * 1.5, radius * 1.5));
            painter.drawLine(QPointF(center.x() - radius * 0.5, center.y() - radius),
                             QPointF(center.x() + radius, center.y() - radius));
            painter.drawLine(QPointF(center.x() + radius, center.y() - radius),
                             QPointF(center.x() + radius, center.y() + radius * 0.5));
        } else {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2.0, radius * 2.0));
        }
        break;
    case DecorationButtonKind::More:
        break;
    }
}

void paintMiniButton(QPainter &painter, const DecorationChrome &chrome,
                     const DecorationFrameVisual &frame,
                     const DecorationButtonVisual &button)
{
    // Miniature stoplights (ADR-0131): an 8 px dot centered in its 12 px hit
    // cell, with its glyph revealed while the handle's controls are hovered.
    // Glyph-style themes color their buttons as console glyphs, so the
    // handlebar keeps classic stoplight colors for them.
    const QPointF center = button.geometry.center();
    if (button.kind == DecorationButtonKind::More) {
        QColor ink = decorationCaptionColor(chrome, frame.active);
        if (!frame.active) {
            ink.setAlphaF(0.7f);
        }
        if (button.hovered || button.pressed) {
            QColor plate = ink;
            plate.setAlpha(button.pressed ? 72 : 40);
            painter.setPen(Qt::NoPen);
            painter.setBrush(plate);
            painter.drawRoundedRect(button.geometry, 3.0, 3.0);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        for (const qreal offset : {-3.0, 0.0, 3.0}) {
            painter.drawEllipse(QPointF(center.x() + offset, center.y()), 1.1, 1.1);
        }
        return;
    }
    QColor fill;
    if (chrome.glyphChrome()) {
        fill = button.kind == DecorationButtonKind::Close ? QColor(QStringLiteral("#f07c76"))
            : button.kind == DecorationButtonKind::Minimize ? QColor(QStringLiteral("#e8bf63"))
                                                             : QColor(QStringLiteral("#71bd8a"));
        if (!frame.active) {
            fill = fill.darker(112);
        }
    } else {
        fill = decorationButtonFill(chrome, button.kind, frame.active);
    }
    if (button.pressed) {
        fill = fill.darker(125);
    } else if (button.hovered) {
        fill = fill.lighter(108);
    }
    constexpr qreal radius = 4.0;
    painter.setPen(QPen(fill.darker(118), 0.6));
    painter.setBrush(fill);
    painter.drawEllipse(center, radius, radius);
    if (!frame.controlsHovered) {
        return;
    }
    QPen pen(qGray(fill.rgb()) >= 128 ? QColor(Qt::black) : QColor(Qt::white), 0.9);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    constexpr qreal glyph = 1.7;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        painter.drawLine(center + QPointF(-glyph, -glyph), center + QPointF(glyph, glyph));
        painter.drawLine(center + QPointF(glyph, -glyph), center + QPointF(-glyph, glyph));
        break;
    case DecorationButtonKind::Minimize:
        painter.drawLine(center + QPointF(-glyph, 0.0), center + QPointF(glyph, 0.0));
        break;
    case DecorationButtonKind::Maximize:
        painter.drawRect(QRectF(center.x() - glyph, center.y() - glyph, glyph * 2.0, glyph * 2.0));
        break;
    case DecorationButtonKind::More:
        break;
    }
}

} // namespace

DecorationButtonSide effectiveButtonSide(const DecorationChrome &chrome)
{
    if (chrome.buttonSide == QLatin1String("left")) {
        return DecorationButtonSide::Left;
    }
    if (chrome.buttonSide == QLatin1String("right")) {
        return DecorationButtonSide::Right;
    }
    return chrome.glyphChrome() || chrome.flatChrome() ? DecorationButtonSide::Right
                                                       : DecorationButtonSide::Left;
}

QList<DecorationButtonKind> decorationButtonKinds(const DecorationChrome &chrome)
{
    if (chrome.buttons == QLatin1String("close")) {
        return {DecorationButtonKind::Close};
    }
    QList<DecorationButtonKind> kinds =
        effectiveButtonSide(chrome) == DecorationButtonSide::Right
        ? QList<DecorationButtonKind>{DecorationButtonKind::Minimize,
                                      DecorationButtonKind::Maximize,
                                      DecorationButtonKind::Close}
        : QList<DecorationButtonKind>{DecorationButtonKind::Close,
                                      DecorationButtonKind::Minimize,
                                      DecorationButtonKind::Maximize};
    if (chrome.buttons == QLatin1String("minimize-close")) {
        kinds.removeAll(DecorationButtonKind::Maximize);
    }
    return kinds;
}

QList<DecorationButtonVisual> layoutDecorationButtons(const DecorationChrome &chrome,
                                                      const QSizeF &size)
{
    QList<DecorationButtonVisual> buttons;
    const auto kinds = decorationButtonKinds(chrome);
    if (kinds.isEmpty()) {
        return buttons;
    }
    const qreal edge = chrome.glyphChrome() || chrome.flatChrome()
        ? DecorationGlyphButtonSize : DecorationClassicButtonSize;
    const auto count = static_cast<qreal>(kinds.size());
    const qreal groupWidth = count * edge + (count - 1.0) * DecorationButtonSpacing;
    qreal x = effectiveButtonSide(chrome) == DecorationButtonSide::Right
        ? size.width() - groupWidth - kRightClusterInset
        : kLeftClusterInset;
    const qreal y = (DecorationTitleHeight - edge) / 2.0;
    for (const auto kind : kinds) {
        buttons.append({kind, QRectF(x, y, edge, edge)});
        x += edge + DecorationButtonSpacing;
    }
    return buttons;
}

QRectF decorationCaptionRect(const DecorationChrome &chrome, const QSizeF &size,
                             const QList<DecorationButtonVisual> &buttons)
{
    qreal left = 12.0;
    qreal right = size.width() - 18.0;
    if (!buttons.isEmpty()) {
        QRectF group = buttons.first().geometry;
        for (const auto &button : buttons) {
            group = group.united(button.geometry);
        }
        if (effectiveButtonSide(chrome) == DecorationButtonSide::Right) {
            right = group.left() - 10.0;
        } else {
            left = group.right() + 18.0;
        }
    }
    return QRectF(left, 0.0, qMax(0.0, right - left), DecorationTitleHeight);
}

void paintDecorationButton(QPainter &painter, const DecorationChrome &chrome,
                           const DecorationFrameVisual &frame,
                           const DecorationButtonVisual &button)
{
    if (!button.visible || button.geometry.isEmpty()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (frame.memberHandle) {
        paintMiniButton(painter, chrome, frame, button);
        painter.restore();
        return;
    }
    const QRectF circle = button.geometry.adjusted(1.0, 1.0, -1.0, -1.0);
    if (chrome.glyphChrome()) {
        paintGlyphChrome(painter, chrome, frame, button, circle);
        painter.restore();
        return;
    }
    if (chrome.flatChrome()) {
        paintFlatButton(painter, chrome, frame, button);
        painter.restore();
        return;
    }
    QColor fill = decorationButtonFill(chrome, button.kind, frame.active);
    if (button.pressed) {
        fill = fill.darker(125);
    } else if (button.hovered) {
        fill = fill.lighter(108);
    }
    painter.setPen(QPen(fill.darker(118), 0.75));
    painter.setBrush(fill);
    painter.drawEllipse(circle);
    if (frame.controlsHovered) {
        paintClassicGlyph(painter, chrome, frame, button, circle);
    }
    painter.restore();
}

} // namespace QindaQt::Decoration
