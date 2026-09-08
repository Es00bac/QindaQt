// SPDX-License-Identifier: LGPL-3.0-or-later
#include "icon_item.h"
#include <qindaqt/controls/application_icon.h>
#include <QGuiApplication>
#include <QEvent>
#include <QPainter>
#include <QQuickWindow>

namespace QindaQt::Controls {
IconItem::IconItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setImplicitWidth(20);
    setImplicitHeight(20);
    setAntialiasing(true);
    qGuiApp->installEventFilter(this);
    connect(this, &QQuickItem::widthChanged, this, [this] { polish(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { polish(); });
    connect(this, &QQuickItem::windowChanged, this, [this] { polish(); });
}
bool IconItem::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ThemeChange
        || event->type() == QEvent::DevicePixelRatioChange)
        polish();
    return QQuickPaintedItem::eventFilter(watched, event);
}
void IconItem::setName(const QString &name) {
    if (m_name == name) return;
    m_name = name;
    emit nameChanged();
    polish();
}
void IconItem::setColor(const QColor &color) {
    if (m_color == color) return;
    m_color = color;
    emit colorChanged();
    polish();
}
void IconItem::updatePolish() {
    // AGENT-GUARD: QQuickPaintedItem::paint may run on the render thread.
    // Theme lookup and QPixmap creation stay in GUI-thread polish; paint reads
    // only the prepared image while Qt synchronizes the item with that thread.
    m_image = {};
    if (width() <= 0 || height() <= 0) { update(); return; }
    const qreal side = qMin(width(), height());
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    const int pixels = qBound(1, qRound(qMin(side, 512.0) * dpr), 2048);
    const auto icon = applicationIcon(m_name);
    if (!icon.isNull()) {
        m_image = icon.pixmap(QSize(pixels, pixels)).toImage();
        if (m_color.isValid()) {
            QPainter tint(&m_image);
            tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tint.fillRect(m_image.rect(), m_color);
        }
    }
    update();
}
void IconItem::paint(QPainter *painter) {
    if (m_image.isNull()) return;
    const qreal side = qMin(width(), height());
    painter->drawImage(QRectF((width()-side)/2, (height()-side)/2, side, side), m_image);
}
}
