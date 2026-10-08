// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>

namespace QindaQt::Viewer {
// The engine owns this QObject through its image-provider registry. setFrame
// and revision belong to the GUI thread; requestImage alone may cross threads.
// Copies let the scene graph retain an old frame; the worker never sees it.
class FrameProvider final : public QObject, public QQuickImageProvider {
    Q_OBJECT
    Q_PROPERTY(quint64 revision READ revision NOTIFY framePublished)
public:
    FrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    quint64 revision() const { return m_revision; }
    void setFrame(const QImage &frame)
    {
        {
            QMutexLocker lock(&m_mutex);
            m_frame = frame;
        }
        // AGENT-GUARD: stage the image before publishing its revision. A QML
        // binding to the controller notifier may run before this staging slot.
        ++m_revision;
        emit framePublished();
    }
    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        QMutexLocker lock(&m_mutex);
        if (size)
            *size = m_frame.size();
        return m_frame;
    }
signals:
    void framePublished();
private:
    QMutex m_mutex;
    QImage m_frame;
    quint64 m_revision = 0;
};
} // namespace QindaQt::Viewer
