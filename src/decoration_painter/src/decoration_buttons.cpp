// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include "qindaqt/decoration_painter/decoration_button_style.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

#include <algorithm>

// Window decoration buttons: arrangement, layout, and dispatch to the one
// style painter (decoration_button_painter.cpp, ADR-0264), plus the
// miniature handlebar stoplights. Split from the title and caption painter
// only for size; the files are one renderer (ADR-0127).
namespace QindaQt::Decoration {
namespace {

constexpr qreal kLeftClusterInset = 12.0;
constexpr qreal kRightClusterInset = 14.0;

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
        // ADR-0139: on a focused member's identity bar the "more" dots use
        // the contrast-safe handlebar ink; the neutral path is unchanged.
        QColor ink = decorationMemberHandleInkColor(chrome, frame);
        if (!frame.active && !(frame.memberFocused && chrome.identityColor.isValid())) {
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
    case DecorationButtonKind::RollUp:
        // A handlebar never offers roll-up: its wheel rolls the container.
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
    // The style's own side: glyph and flat on the right, the lights on the
    // left, as they always sat (ADR-0129); W19 styles name theirs.
    return decorationButtonStyle(chrome.buttonStyle).side;
}

QList<DecorationButtonKind> decorationButtonKinds(const DecorationChrome &chrome)
{
    const bool right = effectiveButtonSide(chrome) == DecorationButtonSide::Right;
    QList<DecorationButtonKind> kinds;
    if (chrome.buttons == QLatin1String("close")) {
        kinds = {DecorationButtonKind::Close};
    } else {
        kinds = right ? QList<DecorationButtonKind>{DecorationButtonKind::Minimize,
                                                    DecorationButtonKind::Maximize,
                                                    DecorationButtonKind::Close}
                      : QList<DecorationButtonKind>{DecorationButtonKind::Close,
                                                    DecorationButtonKind::Minimize,
                                                    DecorationButtonKind::Maximize};
        if (chrome.buttons == QLatin1String("minimize-close")) {
            kinds.removeAll(DecorationButtonKind::Maximize);
        }
    }
    // ADR-0264: roll-up sits at the cluster's inner end, away from close.
    if (chrome.rollUpButton) {
        if (right) {
            kinds.prepend(DecorationButtonKind::RollUp);
        } else {
            kinds.append(DecorationButtonKind::RollUp);
        }
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
    const auto &style = decorationButtonStyle(chrome.buttonStyle);
    const QSizeF cell = decorationButtonCell(chrome);
    const qreal gap = decorationButtonGap(chrome);
    const auto count = static_cast<qreal>(kinds.size());
    const qreal groupWidth = count * cell.width() + (count - 1.0) * gap;
    qreal x = effectiveButtonSide(chrome) == DecorationButtonSide::Right
        ? size.width() - groupWidth - (style.flush ? 0.0 : kRightClusterInset)
        : (style.flush ? 0.0 : kLeftClusterInset);
    const qreal y = (decorationTitleHeight(chrome) - cell.height()) / 2.0;
    for (const auto kind : kinds) {
        buttons.append({kind, QRectF(QPointF(x, y), cell)});
        x += cell.width() + gap;
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
    return QRectF(left, 0.0, qMax(0.0, right - left), decorationTitleHeight(chrome));
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
    const auto &style = decorationButtonStyle(chrome.buttonStyle);
    const auto kinds = decorationButtonKinds(chrome);
    DecorationStyledButton styled;
    styled.kind = button.kind;
    styled.geometry = button.geometry;
    styled.active = frame.active;
    styled.hovered = button.hovered;
    styled.pressed = button.pressed;
    // Traffic-light glyphs appear only while any control is hovered.
    styled.glyphVisible = !style.glyphsOnHover || frame.controlsHovered;
    styled.restoreGlyph = frame.restoreGlyph;
    styled.index = std::max<qsizetype>(0, kinds.indexOf(button.kind));
    styled.count = std::max<qsizetype>(1, kinds.size());
    paintStyledButton(painter, style, chrome, styled);
    painter.restore();
}

} // namespace QindaQt::Decoration
