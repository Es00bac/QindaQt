// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "region_geometry.h"

#include <QImage>
#include <QList>
#include <QMutex>
#include <QQuickImageProvider>
#include <QRect>

namespace QindaQt::Screenshot {

// The pixels QML shows: the current result and, while a region is being
// chosen, the frozen workspace cut per output.
//
// AGENT-GUARD: the image provider reads from QML's loader thread while the
// GUI thread replaces images, so every access takes the mutex. QImage is
// implicitly shared; copies handed out under the lock are safe to use after.
// Pixels live only in memory and are dropped when the flow ends.
class FrameStore final {
public:
    void setResult(const QImage &image);
    [[nodiscard]] QImage result() const;
    [[nodiscard]] int resultRevision() const;

    // `screens` are logical output rectangles in the order QML lists
    // Qt.application.screens.
    void setWorkspace(const WorkspaceFrame &frame, const QList<QRect> &screens);
    void clearWorkspace();
    [[nodiscard]] WorkspaceFrame workspace() const;
    [[nodiscard]] QImage screenImage(qsizetype index) const;
    [[nodiscard]] int workspaceRevision() const;

private:
    mutable QMutex m_mutex;
    QImage m_result;
    int m_resultRevision = 0;
    WorkspaceFrame m_workspace;
    QList<QRect> m_screens;
    int m_workspaceRevision = 0;
};

// Serves `image://capture/result/<revision>` and
// `image://capture/screen/<revision>/<index>`. The revision only defeats
// QML's pixmap cache; the store always answers with its current image.
class CaptureImageProvider final : public QQuickImageProvider {
public:
    explicit CaptureImageProvider(const FrameStore &store);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    const FrameStore &m_store;
};

} // namespace QindaQt::Screenshot
