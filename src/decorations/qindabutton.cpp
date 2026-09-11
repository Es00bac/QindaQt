// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindabutton.h"

#include "qindadecoration.h"

#include <KDecoration3/DecoratedWindow>

#include <QBrush>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace QindaQt::Decoration {

using KDecoration3::DecorationButtonType;

QindaButton::QindaButton(DecorationButtonType type,
                         QindaDecoration *decoration,
                         QObject *parent)
    : KDecoration3::DecorationButton(type, decoration, parent)
{
    connect(this, &KDecoration3::DecorationButton::hoveredChanged,
            decoration, &QindaDecoration::updateControlHover);
}

QindaButton::QindaButton(QObject *parent, const QVariantList &args)
    : QindaButton(args.value(0).value<DecorationButtonType>(),
                  qobject_cast<QindaDecoration *>(
                      args.value(1).value<KDecoration3::Decoration *>()),
                  parent)
{
    setGeometry(QRectF(0.0, 0.0, 14.0, 14.0));
}

QindaButton *QindaButton::create(DecorationButtonType type,
                                 KDecoration3::Decoration *decoration,
                                 QObject *parent)
{
    auto *qinda = qobject_cast<QindaDecoration *>(decoration);
    if (!qinda) {
        return nullptr;
    }
    auto *button = new QindaButton(type, qinda, parent);
    auto *window = decoration->window();
    switch (type) {
    case DecorationButtonType::Close:
        button->setVisible(window->isCloseable());
        QObject::connect(window, &KDecoration3::DecoratedWindow::closeableChanged,
                         button, &QindaButton::setVisible);
        break;
    case DecorationButtonType::Minimize:
        button->setVisible(window->isMinimizeable());
        QObject::connect(window, &KDecoration3::DecoratedWindow::minimizeableChanged,
                         button, &QindaButton::setVisible);
        break;
    case DecorationButtonType::Maximize:
        button->setVisible(window->isMaximizeable());
        QObject::connect(window, &KDecoration3::DecoratedWindow::maximizeableChanged,
                         button, &QindaButton::setVisible);
        break;
    default:
        button->setVisible(false);
        break;
    }
    return button;
}

void QindaButton::paint(QPainter *painter, const QRectF &repaintArea)
{
    Q_UNUSED(repaintArea)
    if (!painter || !decoration() || !isVisible()) {
        return;
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    auto circle = geometry().adjusted(1.0, 1.0, -1.0, -1.0);
    const auto *qinda = qobject_cast<const QindaDecoration *>(decoration());
    if (qinda && qinda->glyphChrome()) {
        paintGlyphChrome(*painter, circle);
        painter->restore();
        return;
    }
    auto fill = fillColor();
    if (isPressed()) {
        fill = fill.darker(125);
    } else if (isHovered()) {
        fill = fill.lighter(108);
    }
    painter->setPen(QPen(fill.darker(118), 0.75));
    painter->setBrush(fill);
    painter->drawEllipse(circle);

    if (qinda && qinda->controlsHovered()) {
        paintGlyph(*painter, circle);
    }
    painter->restore();
}

QColor QindaButton::fillColor() const
{
    const bool active = decoration() && decoration()->window()->isActive();
    const auto *qinda = qobject_cast<const QindaDecoration *>(decoration());
    QColor color = qinda ? qinda->buttonColor(type())
                         : QColor(QStringLiteral("#8da19a"));
    return active ? color : color.darker(112);
}

void QindaButton::paintGlyph(QPainter &painter, const QRectF &circle) const
{
    const auto *qinda = qobject_cast<const QindaDecoration *>(decoration());
    QPen pen(qinda ? qinda->buttonGlyphColor(type())
                   : QColor(QStringLiteral("#26312d")));
    pen.setWidthF(1.15);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = circle.center();
    const qreal radius = circle.width() * 0.20;
    switch (type()) {
    case DecorationButtonType::Close:
        painter.drawLine(center + QPointF(-radius, -radius),
                         center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius),
                         center + QPointF(-radius, radius));
        break;
    case DecorationButtonType::Minimize:
        painter.drawLine(center + QPointF(-radius, radius * 0.45),
                         center + QPointF(radius, radius * 0.45));
        break;
    case DecorationButtonType::Maximize:
        if (decoration()->window()->isMaximized()
            || (qinda && qinda->memberFocusMaximized())) {
            painter.drawRect(QRectF(center.x() - radius,
                                    center.y() - radius * 0.55,
                                    radius * 1.45, radius * 1.45));
            painter.drawRect(QRectF(center.x() - radius * 0.45,
                                    center.y() - radius,
                                    radius * 1.45, radius * 1.45));
        } else {
            painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2.0, radius * 2.0));
        }
        break;
    default:
        break;
    }
}

void QindaButton::paintGlyphChrome(QPainter &painter, const QRectF &circle) const
{
    // AGENT-NOTE: glyph chrome draws the console-style outline glyphs in their
    // own colors directly on the Luna paint. The dash-pattern stroke reads as
    // chipped paint at 16 px; the shapes stay recognizable when eroded.
    const auto *qinda = qobject_cast<const QindaDecoration *>(decoration());
    QColor stroke = qinda
        ? qinda->glyphChromeColor(type())
        : QColor(QStringLiteral("#8da19a"));
    if (decoration() && !decoration()->window()->isActive()) {
        auto dimmed = stroke.toHsl();
        dimmed.setHslF(dimmed.hslHueF(),
                       dimmed.saturationF() * 0.45f,
                       qBound(0.0f, static_cast<float>(dimmed.lightnessF() * 0.9), 1.0f),
                       stroke.alphaF());
        stroke = dimmed;
    }
    if (isPressed()) {
        stroke = stroke.darker(130);
    } else if (isHovered()) {
        stroke = stroke.lighter(115);
    }

    if (isHovered() || isPressed()) {
        const QColor halo = isPressed() ? QColor(0, 0, 0, 60)
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
    const bool maximized = decoration() && decoration()->window()->isMaximized();
    switch (type()) {
    case DecorationButtonType::Close:
        painter.drawLine(center + QPointF(-radius, -radius),
                         center + QPointF(radius, radius));
        painter.drawLine(center + QPointF(radius, -radius),
                         center + QPointF(-radius, radius));
        break;
    case DecorationButtonType::Minimize:
        painter.drawEllipse(center, radius, radius);
        break;
    case DecorationButtonType::Maximize:
        if (maximized || (qinda && qinda->memberFocusMaximized())) {
            painter.drawRect(QRectF(center.x() - radius,
                                    center.y() - radius,
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
    default:
        break;
    }
}

} // namespace QindaQt::Decoration
