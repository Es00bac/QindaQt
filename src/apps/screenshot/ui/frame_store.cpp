// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame_store.h"

#include <QMutexLocker>

namespace QindaQt::Screenshot {

void FrameStore::setResult(const QImage &image)
{
    const QMutexLocker lock(&m_mutex);
    m_result = image;
    ++m_resultRevision;
}

QImage FrameStore::result() const
{
    const QMutexLocker lock(&m_mutex);
    return m_result;
}

int FrameStore::resultRevision() const
{
    const QMutexLocker lock(&m_mutex);
    return m_resultRevision;
}

void FrameStore::setWorkspace(const WorkspaceFrame &frame, const QList<QRect> &screens)
{
    const QMutexLocker lock(&m_mutex);
    m_workspace = frame;
    m_screens = screens;
    ++m_workspaceRevision;
}

void FrameStore::clearWorkspace()
{
    const QMutexLocker lock(&m_mutex);
    m_workspace = {};
    m_screens.clear();
    ++m_workspaceRevision;
}

WorkspaceFrame FrameStore::workspace() const
{
    const QMutexLocker lock(&m_mutex);
    return m_workspace;
}

QImage FrameStore::screenImage(qsizetype index) const
{
    const QMutexLocker lock(&m_mutex);
    if (index < 0 || index >= m_screens.size())
        return {};
    return cropRegion(m_workspace, m_screens.at(index));
}

int FrameStore::workspaceRevision() const
{
    const QMutexLocker lock(&m_mutex);
    return m_workspaceRevision;
}

CaptureImageProvider::CaptureImageProvider(const FrameStore &store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(store)
{
}

QImage CaptureImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QStringList parts = id.split(QLatin1Char('/'));
    QImage image;
    if (parts.value(0) == QLatin1String("result")) {
        image = m_store.result();
    } else if (parts.value(0) == QLatin1String("screen") && parts.size() == 3) {
        bool ok = false;
        const qsizetype index = parts.at(2).toLongLong(&ok);
        if (ok)
            image = m_store.screenImage(index);
    }
    if (size)
        *size = image.size();
    if (!image.isNull() && requestedSize.isValid() && !requestedSize.isEmpty())
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

} // namespace QindaQt::Screenshot
