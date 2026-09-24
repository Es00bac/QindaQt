// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_button_style.h"

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>

// The one window-button painter (ADR-0264): a plate, a hover treatment and
// a glyph, each chosen by the style row. The pre-W19 painters are the
// Tint/Classic (lights), Halo/Console (glyph) and Plate/Lines (flat) paths.
// AGENT-GUARD: keep those paths issuing exactly the painter calls they did,
// in the same order, or the shipped styles stop reproducing their pixels;
// tst_decoration_button_styles compares them with a copy of the old code.
namespace QindaQt::Decoration {
namespace {

// The color a plate starts from, before focus and hover adjust it.
QColor plateBase(const DecorationButtonStyle &style, const DecorationChrome &chrome,
                 const DecorationStyledButton &button)
{
    switch (style.colors) {
    case DecorationButtonColors::Surface:
        return decorationTitleColor(chrome, button.active);
    case DecorationButtonColors::Own:
        return button.kind == DecorationButtonKind::Close && style.closeFace.isValid()
            ? style.closeFace : style.face;
    case DecorationButtonColors::Action:
        break;
    }
    return decorationButtonFill(chrome, button.kind, true);
}

QColor glyphInk(const DecorationButtonStyle &style, const DecorationChrome &chrome,
                const DecorationStyledButton &button, const QColor &base)
{
    switch (style.ink) {
    case DecorationGlyphInk::Contrast:
        return qGray(base.rgb()) >= 128 ? QColor(Qt::black) : QColor(Qt::white);
    case DecorationGlyphInk::Caption: {
        // Caption ink keeps monochrome glyphs legible on classic and worn
        // Luna title bars alike.
        QColor ink = decorationCaptionColor(chrome, button.active);
        if (!button.active) {
            ink.setAlphaF(0.72f);
        }
        return ink;
    }
    case DecorationGlyphInk::Action:
        break;
    }
    QColor stroke = decorationGlyphChromeColor(chrome, button.kind, button.restoreGlyph);
    if (!button.active) {
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
    return stroke;
}

void drawShape(QPainter &painter, DecorationButtonShape shape, const QRectF &rect, qreal radius)
{
    switch (shape) {
    case DecorationButtonShape::Circle:
    case DecorationButtonShape::Dot:
        painter.drawEllipse(rect);
        return;
    case DecorationButtonShape::RoundedSquare:
        painter.drawRoundedRect(rect, radius, radius);
        return;
    case DecorationButtonShape::Square:
        painter.drawRect(rect);
        return;
    case DecorationButtonShape::None:
    case DecorationButtonShape::Pill:
        return;
    }
}

// A raised face with a light top-left and a dark bottom-right edge; a
// pressed button is sunken, its edges swapped.
void paintBevel(QPainter &painter, const QRectF &plate, const QColor &face, bool sunken)
{
    painter.setPen(QPen(face.darker(165), 1.0));
    painter.setBrush(face);
    painter.drawRect(plate.adjusted(0.5, 0.5, -0.5, -0.5));
    const QRectF inner = plate.adjusted(1.5, 1.5, -1.5, -1.5);
    const QColor light(255, 255, 255, 150);
    const QColor shade(0, 0, 0, 90);
    painter.setPen(QPen(sunken ? shade : light, 1.0));
    painter.drawLine(inner.bottomLeft(), inner.topLeft());
    painter.drawLine(inner.topLeft(), inner.topRight());
    painter.setPen(QPen(sunken ? light : shade, 1.0));
    painter.drawLine(inner.topRight(), inner.bottomRight());
    painter.drawLine(inner.bottomRight(), inner.bottomLeft());
}

void paintFilledPlate(QPainter &painter, const DecorationButtonStyle &style,
                      const DecorationStyledButton &button, const QRectF &plate,
                      const QColor &fill)
{
    switch (style.fill) {
    case DecorationButtonFill::Solid:
        painter.setPen(QPen(fill.darker(118), 0.75));
        painter.setBrush(fill);
        drawShape(painter, style.shape, plate, style.radius);
        return;
    case DecorationButtonFill::Gradient: {
        QLinearGradient body(plate.topLeft(), plate.bottomLeft());
        body.setColorAt(0.0, fill.lighter(122));
        body.setColorAt(1.0, fill.darker(115));
        painter.setPen(QPen(fill.darker(140), 0.8));
        painter.setBrush(body);
        drawShape(painter, style.shape, plate, style.radius);
        // A thin inner light edge keeps a tile readable on its own color.
        painter.setPen(QPen(QColor(255, 255, 255, 110), 1.0));
        painter.setBrush(Qt::NoBrush);
        drawShape(painter, style.shape, plate.adjusted(1.0, 1.0, -1.0, -1.0),
                  std::max(0.0, style.radius - 1.0));
        return;
    }
    case DecorationButtonFill::Glass: {
        QLinearGradient body(plate.topLeft(), plate.bottomLeft());
        body.setColorAt(0.0, fill.lighter(135));
        body.setColorAt(0.55, fill);
        body.setColorAt(1.0, fill.darker(108));
        painter.setPen(QPen(fill.darker(130), 0.8));
        painter.setBrush(body);
        drawShape(painter, style.shape, plate, style.radius);
        // The gel's gloss: a soft white lens over the upper half.
        const QRectF gloss(plate.left() + plate.width() * 0.18,
                           plate.top() + plate.height() * 0.06, plate.width() * 0.64,
                           plate.height() * 0.46);
        QLinearGradient shine(gloss.topLeft(), gloss.bottomLeft());
        shine.setColorAt(0.0, QColor(255, 255, 255, 190));
        shine.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(shine);
        painter.drawEllipse(gloss);
        return;
    }
    case DecorationButtonFill::Outline: {
        painter.setPen(QPen(fill, 1.4));
        if (button.hovered || button.pressed) {
            QColor wash = fill;
            wash.setAlpha(button.pressed ? 120 : 70);
            painter.setBrush(wash);
        } else {
            painter.setBrush(Qt::NoBrush);
        }
        drawShape(painter, style.shape, plate.adjusted(0.7, 0.7, -0.7, -0.7), style.radius);
        return;
    }
    case DecorationButtonFill::Bevel:
        paintBevel(painter, plate, fill, button.pressed);
        return;
    }
}

// Tint: a plate at rest that lightens under the pointer and darkens when
// pressed (the traffic lights, gel, bevel, tiles, dots, rings, chunky).
void paintRestingPlate(QPainter &painter, const DecorationButtonStyle &style,
                       const DecorationStyledButton &button, const QRectF &inset,
                       const QColor &base)
{
    if (style.shape == DecorationButtonShape::None
        || style.shape == DecorationButtonShape::Pill) {
        return;
    }
    // The title surface already follows focus; action and own colors dim.
    QColor fill = style.colors != DecorationButtonColors::Surface && !button.active
        ? base.darker(112) : base;
    if (style.fill == DecorationButtonFill::Bevel) {
        fill = button.pressed ? fill.darker(106) : button.hovered ? fill.lighter(106) : fill;
    } else if (button.pressed) {
        fill = fill.darker(125);
    } else if (button.hovered) {
        fill = fill.lighter(108);
    }
    QRectF plate = inset;
    if (style.shape == DecorationButtonShape::Dot && !button.glyphVisible) {
        const qreal radius = std::min(inset.width(), inset.height()) * 0.28;
        plate = QRectF(inset.center() - QPointF(radius, radius), QSizeF(radius, radius) * 2.0);
    }
    paintFilledPlate(painter, style, button, plate, fill);
}

// A joined capsule's share of one cell: the capsule spans the cluster, and
// each button fills only the part inside its own cell.
void fillPillSegment(QPainter &painter, const DecorationStyledButton &button, const QColor &color)
{
    const QRectF cell = button.geometry;
    const QRectF cluster(cell.left() - static_cast<qreal>(button.index) * cell.width(),
                         cell.top(), static_cast<qreal>(button.count) * cell.width(),
                         cell.height());
    QPainterPath capsule;
    capsule.addRoundedRect(cluster, cell.height() / 2.0, cell.height() / 2.0);
    QPainterPath own;
    own.addRect(cell);
    painter.fillPath(capsule.intersected(own), color);
}

// Plate: a caption-ink plate under the pointer (at rest too when the style
// has a rest alpha); the close may take its own color, with contrast ink.
// Returns that contrast ink, or an invalid color to keep the style's ink.
QColor paintHoverPlate(QPainter &painter, const DecorationButtonStyle &style,
                       const DecorationChrome &chrome, const DecorationStyledButton &button)
{
    const bool engaged = button.hovered || button.pressed;
    if (!engaged && style.restAlpha <= 0) {
        return {};
    }
    QColor plate;
    QColor ink;
    if (engaged && style.closeHoverColor && button.kind == DecorationButtonKind::Close
        && chrome.close.isValid()) {
        plate = button.pressed ? chrome.close.darker(118) : chrome.close;
        ink = qGray(plate.rgb()) < 150 ? QColor(Qt::white) : QColor(Qt::black);
    } else {
        plate = decorationCaptionColor(chrome, button.active);
        plate.setAlpha(button.pressed ? std::max(72, style.restAlpha + 56)
                       : button.hovered ? std::max(40, style.restAlpha + 28)
                                        : style.restAlpha);
    }
    if (style.shape == DecorationButtonShape::Pill) {
        fillPillSegment(painter, button, plate);
        return ink;
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(plate);
    painter.drawRoundedRect(button.geometry, style.radius, style.radius);
    return ink;
}

// Halo: a soft glow behind a plate-less glyph under the pointer.
void paintHalo(QPainter &painter, const DecorationStyledButton &button, const QRectF &inset)
{
    if (!button.hovered && !button.pressed) {
        return;
    }
    const QColor halo = button.pressed ? QColor(0, 0, 0, 60) : QColor(255, 255, 255, 46);
    painter.setPen(Qt::NoPen);
    painter.setBrush(halo);
    painter.drawRoundedRect(inset.adjusted(-1.0, -1.0, -1.0, -1.0), 3.5, 3.5);
}

void drawCross(QPainter &painter, const QPointF &center, qreal radius)
{
    painter.drawLine(center + QPointF(-radius, -radius), center + QPointF(radius, radius));
    painter.drawLine(center + QPointF(radius, -radius), center + QPointF(-radius, radius));
}

// Roll-up: an upward chevron, the window folding up toward its title.
void drawRollUpChevron(QPainter &painter, const QPointF &center, qreal radius)
{
    QPainterPath chevron;
    chevron.moveTo(center + QPointF(-radius, radius * 0.45));
    chevron.lineTo(center + QPointF(0.0, -radius * 0.45));
    chevron.lineTo(center + QPointF(radius, radius * 0.45));
    painter.drawPath(chevron);
}

void paintClassicGlyph(QPainter &painter, const DecorationButtonStyle &style,
                       const DecorationStyledButton &button, const QRectF &inset,
                       const QColor &ink)
{
    QPen pen(ink);
    pen.setWidthF(style.stroke);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = inset.center();
    const qreal radius = std::min(inset.width(), inset.height()) * 0.20;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        drawCross(painter, center, radius);
        break;
    case DecorationButtonKind::Minimize:
        painter.drawLine(center + QPointF(-radius, radius * 0.45),
                         center + QPointF(radius, radius * 0.45));
        break;
    case DecorationButtonKind::Maximize:
        if (button.restoreGlyph) {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius * 0.55,
                                    radius * 1.45, radius * 1.45));
            painter.drawRect(QRectF(center.x() - radius * 0.45, center.y() - radius,
                                    radius * 1.45, radius * 1.45));
        } else {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2.0, radius * 2.0));
        }
        break;
    case DecorationButtonKind::RollUp:
        drawRollUpChevron(painter, center, radius);
        break;
    case DecorationButtonKind::More:
        break;
    }
}

void paintLinesGlyph(QPainter &painter, const DecorationButtonStyle &style,
                     const DecorationStyledButton &button, const QColor &ink)
{
    const QRectF box = button.geometry;
    QPen pen(ink, style.stroke);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QPointF center = box.center();
    const qreal radius = std::min(box.width(), box.height()) * 0.24;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        drawCross(painter, center, radius);
        break;
    case DecorationButtonKind::Minimize:
        painter.drawLine(center + QPointF(-radius, 0.0), center + QPointF(radius, 0.0));
        break;
    case DecorationButtonKind::Maximize:
        if (button.restoreGlyph) {
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
    case DecorationButtonKind::RollUp:
        drawRollUpChevron(painter, center, radius);
        break;
    case DecorationButtonKind::More:
        break;
    }
}

void paintConsoleGlyph(QPainter &painter, const DecorationButtonStyle &style,
                       const DecorationStyledButton &button, const QRectF &inset,
                       const QColor &ink)
{
    // ADR-0264: solid strokes. The pre-W19 dashed "worn" pen broke the
    // close cross into fragments that no longer read as a cross; the
    // circle, triangle and square lost their outlines the same way.
    // Keep: triangle = maximize, square = restore (the owner's choice).
    QPen pen(QBrush(ink), style.stroke);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = inset.center();
    const qreal radius = std::min(inset.width(), inset.height()) * 0.30;
    switch (button.kind) {
    case DecorationButtonKind::Close:
        drawCross(painter, center, radius);
        break;
    case DecorationButtonKind::Minimize:
        painter.drawEllipse(center, radius, radius);
        break;
    case DecorationButtonKind::Maximize:
        if (button.restoreGlyph) {
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
    case DecorationButtonKind::RollUp:
        drawRollUpChevron(painter, center, radius);
        break;
    case DecorationButtonKind::More:
        break;
    }
}

} // namespace

void paintStyledButton(QPainter &painter, const DecorationButtonStyle &style,
                       const DecorationChrome &chrome, const DecorationStyledButton &button)
{
    const QRectF inset = button.geometry.adjusted(1.0, 1.0, -1.0, -1.0);
    const QColor base = plateBase(style, chrome, button);
    QColor ink = glyphInk(style, chrome, button, base);
    switch (style.hover) {
    case DecorationButtonHover::Tint:
        paintRestingPlate(painter, style, button, inset, base);
        break;
    case DecorationButtonHover::Plate:
        if (const QColor contrast = paintHoverPlate(painter, style, chrome, button);
            contrast.isValid()) {
            ink = contrast;
        }
        break;
    case DecorationButtonHover::Halo:
        paintHalo(painter, button, inset);
        break;
    }
    if (!button.glyphVisible) {
        return;
    }
    switch (style.glyphs) {
    case DecorationGlyphFamily::Classic:
        paintClassicGlyph(painter, style, button, inset, ink);
        return;
    case DecorationGlyphFamily::Lines:
        paintLinesGlyph(painter, style, button, ink);
        return;
    case DecorationGlyphFamily::Console:
        paintConsoleGlyph(painter, style, button, inset, ink);
        return;
    }
}

void paintContainerButton(QPainter &painter, const HybridChrome::ChromeRenderPlan &plan,
                          const HybridChrome::WindowButtonGeometry &button, bool hovered,
                          bool pressed, bool glyphVisible)
{
    using HybridChrome::WindowAction;
    // The container row is the palette's raised surface, exactly what a
    // window title with no authored title color paints, so the palette
    // stands in for the chrome and every caption-ink rule carries over.
    const auto &palette = plan.style.palette;
    DecorationChrome chrome;
    chrome.surface = palette.surface;
    chrome.surfaceRaised = palette.surfaceRaised;
    chrome.border = palette.border;
    chrome.text = palette.text;
    chrome.textMuted = palette.textMuted;
    chrome.close = palette.close;
    chrome.minimize = palette.minimize;
    chrome.maximize = palette.maximize;
    chrome.buttonStyle = plan.style.namedButtonStyle;
    DecorationStyledButton styled;
    styled.kind = button.action == WindowAction::Close ? DecorationButtonKind::Close
        : button.action == WindowAction::Minimize      ? DecorationButtonKind::Minimize
                                                       : DecorationButtonKind::Maximize;
    styled.restoreGlyph = button.action == WindowAction::Restore;
    styled.geometry = button.rect;
    styled.hovered = hovered;
    styled.pressed = pressed;
    styled.glyphVisible = glyphVisible;
    for (qsizetype index = 0; index < plan.buttons.size(); ++index) {
        if (plan.buttons.at(index).action == button.action) {
            styled.index = index;
        }
    }
    styled.count = std::max<qsizetype>(1, plan.buttons.size());
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    paintStyledButton(painter, decorationButtonStyle(chrome.buttonStyle), chrome, styled);
    painter.restore();
}

} // namespace QindaQt::Decoration
