// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMutex>
#include <QQuickImageProvider>

namespace QindaQt::Viewer {
// The QML engine owns this provider. Copies let the scene graph retain its old
// frame while the GUI publishes a newly rendered one; the worker never sees it.
class FrameProvider final : public QQuickImageProvider {
public:
    FrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    void setFrame(const QImage &frame)
    {
        QMutexLocker lock(&m_mutex);
        m_frame = frame;
    }
    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        QMutexLocker lock(&m_mutex);
        if (size)
            *size = m_frame.size();
        return m_frame;
    }
private:
    QMutex m_mutex;
    QImage m_frame;
};
} // namespace QindaQt::Viewer
