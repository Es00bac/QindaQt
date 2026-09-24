// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// AGENT-GUARD: a frozen copy of the three window-button painters as they
// shipped before ADR-0264, kept only as the reference the data-driven style
// painter must reproduce pixel for pixel (tst_decoration_button_styles).
// The one deliberate difference is the W19 fix itself: the glyph set's
// dashed "worn" pen is solid here, as it is in production. Never edit this
// copy to make a failing comparison pass; fix the style painter instead.
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace LegacyButtons {

using namespace QindaQt::Decoration;

inline void paintClassicGlyph(QPainter &painter, const DecorationChrome &chrome,
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
        painter.drawLine(center + QPointF(-radius, -radius), center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius), center + QPointF(-radius, radius));
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
    case DecorationButtonKind::RollUp:
        break;
    }
}

inline void paintGlyphChrome(QPainter &painter, const DecorationChrome &chrome,
                             const DecorationFrameVisual &frame,
                             const DecorationButtonVisual &button, const QRectF &circle)
{
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
        const QColor halo = button.pressed ? QColor(0, 0, 0, 60) : QColor(255, 255, 255, 46);
        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawRoundedRect(circle.adjusted(-1.0, -1.0, -1.0, -1.0), 3.5, 3.5);
    }
    // The shipped pen also set a {5.0, 1.6, 3.0, 1.2} dash pattern; ADR-0264
    // removed it, and this reference follows.
    QPen pen(QBrush(stroke), 1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = circle.center();
    const qreal radius = circle.width() * 0.30;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        painter.drawLine(center + QPointF(-radius, -radius), center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius), center + QPointF(-radius, radius));
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
    case DecorationButtonKind::RollUp:
        break;
    }
}

inline void paintFlatButton(QPainter &painter, const DecorationChrome &chrome,
                            const DecorationFrameVisual &frame,
                            const DecorationButtonVisual &button)
{
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
        painter.drawLine(center + QPointF(-radius, -radius), center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius), center + QPointF(-radius, radius));
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
    case DecorationButtonKind::RollUp:
        break;
    }
}

// The shipped paintDecorationButton for a window (not handlebar) button.
inline void paintButton(QPainter &painter, const DecorationChrome &chrome,
                        const DecorationFrameVisual &frame, const DecorationButtonVisual &button)
{
    if (!button.visible || button.geometry.isEmpty()) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
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

} // namespace LegacyButtons
