// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMutex>
#include <QObject>
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
// GUI-thread publisher over an engine-owned provider. The provider must remain
// alive for every setFrame call; composition parents this object to the engine.
// Keep publication separate from provider inheritance across supported Qt APIs.
class FramePublication final : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint64 revision READ revision NOTIFY framePublished)
public:
    explicit FramePublication(FrameProvider &provider, QObject *parent = nullptr)
        : QObject(parent), m_provider(provider) {}
    quint64 revision() const { return m_revision; }
    void setFrame(const QImage &frame)
    {
        // AGENT-GUARD: stage the image before publishing its revision. A QML
        // binding to the controller notifier may run before this staging slot.
        m_provider.setFrame(frame);
        ++m_revision;
        emit framePublished();
    }
signals:
    void framePublished();
private:
    FrameProvider &m_provider;
    quint64 m_revision = 0;
};
} // namespace QindaQt::Viewer
