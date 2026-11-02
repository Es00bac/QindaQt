// SPDX-License-Identifier: GPL-3.0-or-later
#include "mirror_window.h"

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <utility>

namespace qindaqt::link_mirror {

MirrorWindow::MirrorWindow(ViewportMapper &mapper) : m_mapper(mapper), m_translator(mapper)
{
    setMinimumSize(QSize(160, 160));
}

void MirrorWindow::setFrame(QImage frame)
{
    m_frame = std::move(frame);
    update();
}

void MirrorWindow::setStatusText(const QString &text)
{
    m_status = text;
    update();
}

QImage MirrorWindow::renderSnapshot(QSize viewSize) const
{
    ViewportMapper mapper = m_mapper;
    mapper.setViewSize(viewSize);
    QImage image(viewSize, QImage::Format_RGB32);
    QPainter painter(&image);
    render(painter, mapper, viewSize);
    return image;
}

void MirrorWindow::render(QPainter &painter, const ViewportMapper &mapper, QSizeF viewSize) const
{
    painter.fillRect(QRectF(QPointF(0, 0), viewSize), Qt::black);
    const QRectF content = mapper.contentRect();
    if (!m_frame.isNull() && !content.isEmpty()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(content, m_frame);
        return;
    }
    if (!m_status.isEmpty()) {
        painter.setPen(QColor(220, 220, 220));
        painter.drawText(QRectF(QPointF(0, 0), viewSize).adjusted(16, 16, -16, -16),
                         Qt::AlignCenter | Qt::TextWordWrap, m_status);
    }
}

void MirrorWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    render(painter, m_mapper, size());
}

void MirrorWindow::resizeEvent(QResizeEvent *event)
{
    QRasterWindow::resizeEvent(event);
    m_mapper.setViewSize(size());
    Q_EMIT viewResized(size(), devicePixelRatio());
}

void MirrorWindow::forward(const QList<QByteArray> &frames)
{
    if (!frames.isEmpty()) {
        Q_EMIT inputFrames(frames);
    }
}

void MirrorWindow::mousePressEvent(QMouseEvent *event)
{
    forward(m_translator.mousePress(event->position(), event->button()));
    event->accept();
}

void MirrorWindow::mouseMoveEvent(QMouseEvent *event)
{
    forward(m_translator.mouseMove(event->position()));
    event->accept();
}

void MirrorWindow::mouseReleaseEvent(QMouseEvent *event)
{
    forward(m_translator.mouseRelease(event->position(), event->button()));
    event->accept();
}

void MirrorWindow::wheelEvent(QWheelEvent *event)
{
    forward(m_translator.wheel(event->position(), event->angleDelta()));
    event->accept();
}

void MirrorWindow::keyPressEvent(QKeyEvent *event)
{
    forward(m_translator.keyPress(event->key(), event->modifiers(), event->text(), event->isAutoRepeat()));
    event->accept();
}

void MirrorWindow::keyReleaseEvent(QKeyEvent *event)
{
    forward(m_translator.keyRelease(event->key(), event->modifiers(), event->isAutoRepeat()));
    event->accept();
}

void MirrorWindow::focusOutEvent(QFocusEvent *event)
{
    // A drag or held key that loses the window must still end on the phone.
    forward(m_translator.releaseAll());
    QRasterWindow::focusOutEvent(event);
}

void MirrorWindow::closeEvent(QCloseEvent *event)
{
    forward(m_translator.releaseAll());
    Q_EMIT closeRequested();
    event->accept();
}

bool MirrorWindow::event(QEvent *event)
{
    if (event->type() == QEvent::DevicePixelRatioChange) {
        Q_EMIT viewResized(size(), devicePixelRatio());
    }
    return QRasterWindow::event(event);
}

} // namespace qindaqt::link_mirror
