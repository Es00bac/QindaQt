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
    case DecorationButtonType::Custom:
        // The contained-window "more" control (ADR-0131).
        button->setVisible(true);
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
    const auto *qinda = qobject_cast<const QindaDecoration *>(decoration());
    if (!qinda) {
        return;
    }
    // AGENT-CONTRACT: the shared painter renders every button (ADR-0127);
    // this method only reports live hover/press state.
    DecorationButtonVisual visual;
    visual.kind = QindaDecoration::buttonKind(type());
    visual.geometry = geometry();
    visual.hovered = isHovered();
    visual.pressed = isPressed();
    paintDecorationButton(*painter, qinda->chromeState(), qinda->frameState(), visual);
}

} // namespace QindaQt::Decoration
