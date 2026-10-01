// SPDX-License-Identifier: GPL-3.0-or-later
#include "color_picker.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
namespace QindaQt::Services::Portal {
ColorPicker::ColorPicker(QImage image, QWidget *parent) : QWidget(parent), m_image(std::move(image)), m_pixel(m_image.width()/2, m_image.height()/2) {
    setObjectName("colorPicker"); setFocusPolicy(Qt::StrongFocus); setMinimumSize(480, 320);
    setAccessibleName(tr("Choose a pixel using the mouse or arrow keys; Enter confirms"));
}
QRect ColorPicker::imageRect() const { const auto size = m_image.size().scaled(this->size(), Qt::KeepAspectRatio); return {QPoint((width()-size.width())/2, (height()-size.height())/2), size}; }
void ColorPicker::paintEvent(QPaintEvent *) {
    QPainter painter(this); painter.fillRect(rect(), palette().window()); const auto target = imageRect(); painter.drawImage(target, m_image);
    const QPoint point(target.x() + m_pixel.x()*target.width()/m_image.width(), target.y() + m_pixel.y()*target.height()/m_image.height());
    painter.setPen(Qt::white); painter.drawEllipse(point, 6, 6); painter.setPen(Qt::black); painter.drawEllipse(point, 7, 7);
}
void ColorPicker::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    const auto target = imageRect(); if (!target.contains(event->position().toPoint())) return;
    m_pixel = {std::clamp((event->position().toPoint().x()-target.x())*m_image.width()/target.width(), 0, m_image.width()-1), std::clamp((event->position().toPoint().y()-target.y())*m_image.height()/target.height(), 0, m_image.height()-1)};
    Q_EMIT chosen(m_image.pixelColor(m_pixel));
}
void ColorPicker::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Left: m_pixel.rx() = std::max(0, m_pixel.x()-1); break;
    case Qt::Key_Right: m_pixel.rx() = std::min(m_image.width()-1, m_pixel.x()+1); break;
    case Qt::Key_Up: m_pixel.ry() = std::max(0, m_pixel.y()-1); break;
    case Qt::Key_Down: m_pixel.ry() = std::min(m_image.height()-1, m_pixel.y()+1); break;
    case Qt::Key_Return: case Qt::Key_Enter: Q_EMIT chosen(m_image.pixelColor(m_pixel)); return;
    default: QWidget::keyPressEvent(event); return;
    }
    update();
}
}
