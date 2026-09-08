// SPDX-License-Identifier: LGPL-3.0-or-later
#include "icon_item.h"
#include <qindaqt/controls/application_icon.h>
#include <QGuiApplication>
#include <QPainter>
#include <QQuickWindow>

namespace QindaQt::Controls {
IconItem::IconItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setImplicitWidth(20);
    setImplicitHeight(20);
    setAntialiasing(true);
    connect(qGuiApp, &QGuiApplication::paletteChanged, this, [this] { update(); });
}
void IconItem::setName(const QString &name) {
    if (m_name == name) return;
    m_name = name;
    emit nameChanged();
    update();
}
void IconItem::setColor(const QColor &color) {
    if (m_color == color) return;
    m_color = color;
    emit colorChanged();
    update();
}
void IconItem::paint(QPainter *painter) {
    if (width() <= 0 || height() <= 0) return;
    const qreal side = qMin(width(), height());
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    // Bound the allocation even when a hostile QML consumer supplies dimensions.
    const int pixels = qBound(1, qRound(qMin(side, 512.0) * dpr), 2048);
    const auto icon = applicationIcon(m_name);
    if (icon.isNull()) return;
    QPixmap pixmap = icon.pixmap(QSize(pixels, pixels));
    if (m_color.isValid()) {
        QPainter tint(&pixmap);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(pixmap.rect(), m_color);
    }
    painter->drawPixmap(QRectF((width()-side)/2, (height()-side)/2, side, side),
                        pixmap, QRectF(pixmap.rect()));
}
}
