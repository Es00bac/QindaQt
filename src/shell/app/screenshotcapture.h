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
    explicit ScreenshotCapture(QString outputPath, QSize expectedSize, QObject *parent = nullptr);

    void start(QQuickWindow &window);

signals:
    void finished(bool succeeded, const QString &message);

private:
    void capture();
    void fail(const QString &message);

    QString m_outputPath;
    QSize m_expectedSize;
    QQuickWindow *m_window = nullptr;
    QTimer m_timeout;
    bool m_captureScheduled = false;
    bool m_finished = false;
};

} // namespace QindaQt::Shell
