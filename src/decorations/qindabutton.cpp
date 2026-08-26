// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindabutton.h"

#include "qindadecoration.h"

#include <KDecoration3/DecoratedWindow>

#include <QPainter>
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
    auto fill = fillColor();
    if (isPressed()) {
        fill = fill.darker(125);
    } else if (isHovered()) {
        fill = fill.lighter(108);
    }
    painter->setPen(QPen(fill.darker(118), 0.75));
    painter->setBrush(fill);
    painter->drawEllipse(circle);

    const auto *qinda = qobject_cast<const QindaDecoration *>(decoration());
    if (qinda && qinda->controlsHovered()) {
        paintGlyph(*painter, circle);
    }
    painter->restore();
}

QColor QindaButton::fillColor() const
{
    const bool active = decoration() && decoration()->window()->isActive();
    QColor color;
    switch (type()) {
    case DecorationButtonType::Close:
        color = QColor(QStringLiteral("#ff5f57"));
        break;
    case DecorationButtonType::Minimize:
        color = QColor(QStringLiteral("#febc2e"));
        break;
    case DecorationButtonType::Maximize:
        color = QColor(QStringLiteral("#28c840"));
        break;
    default:
        color = QColor(QStringLiteral("#8da19a"));
        break;
    }
    return active ? color : color.darker(112);
}

void QindaButton::paintGlyph(QPainter &painter, const QRectF &circle) const
{
    QPen pen(QColor(QStringLiteral("#26312d")));
    pen.setWidthF(1.15);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto center = circle.center();
    const qreal radius = circle.width() * 0.20;
    const auto *qinda = qobject_cast<const QindaDecoration *>(decoration());
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

} // namespace QindaQt::Decoration
