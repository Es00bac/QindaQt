// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QTimer>

class QQuickWindow;

namespace QindaQt::Shell {

class ScreenshotCapture final : public QObject {
    Q_OBJECT

public:
    // Same GUI thread as the window. A capture owns no window: it must remain
    // alive through finished(). Size is the requested logical window geometry;
    // successful PNG export preserves native physical pixels at the window DPR.
    // One start per object; geometry/readback/write errors emit finished(false).
    explicit ScreenshotCapture(QString outputPath, QSize expectedLogicalSize, QObject *parent = nullptr);

    void start(QQuickWindow &window);

signals:
    void finished(bool succeeded, const QString &message);

private:
    void capture();
    void fail(const QString &message);

    QString m_outputPath;
    QSize m_expectedLogicalSize;
    QQuickWindow *m_window = nullptr;
    QTimer m_timeout;
    bool m_captureScheduled = false;
    bool m_finished = false;
};

} // namespace QindaQt::Shell
