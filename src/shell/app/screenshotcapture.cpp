// SPDX-License-Identifier: GPL-3.0-or-later
#include "screenshotcapture.h"
#include "capturegeometry.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QQuickWindow>
#include <QTimer>

#include <utility>

namespace QindaQt::Shell {
ScreenshotCapture::ScreenshotCapture(QString outputPath, QSize expectedLogicalSize, QObject *parent)
    : QObject(parent)
    , m_outputPath(std::move(outputPath))
    , m_expectedLogicalSize(expectedLogicalSize)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(10'000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("Timed out waiting for the preview frame"));
    });
}

void ScreenshotCapture::start(QQuickWindow &window)
{
    if (m_window != nullptr || m_finished) {
        return;
    }
    if (!CaptureGeometry::physicalSize(m_expectedLogicalSize, window.effectiveDevicePixelRatio())) {
        fail(QStringLiteral("Capture geometry exceeds the bounded native image contract"));
        return;
    }
    if (window.size() != m_expectedLogicalSize) {
        fail(QStringLiteral("Preview logical window size changed before capture"));
        return;
    }
    m_window = &window;
    connect(&window, &QQuickWindow::frameSwapped, this, [this] {
        if (m_captureScheduled || m_finished) {
            return;
        }
        m_captureScheduled = true;
        QTimer::singleShot(0, this, &ScreenshotCapture::capture);
    });
    m_timeout.start();
    window.requestUpdate();
}

void ScreenshotCapture::capture()
{
    if (m_finished || m_window == nullptr) {
        return;
    }

    const auto expectedPhysicalSize =
        CaptureGeometry::physicalSize(m_expectedLogicalSize, m_window->effectiveDevicePixelRatio());
    if (!expectedPhysicalSize) {
        fail(QStringLiteral("Capture geometry exceeds the bounded native image contract"));
        return;
    }
    if (m_window->size() != m_expectedLogicalSize) {
        fail(QStringLiteral("Preview logical window size changed before capture"));
        return;
    }
    const QImage image = m_window->grabWindow();
    if (image.isNull()) {
        fail(QStringLiteral("Qt Quick returned an empty screenshot"));
        return;
    }
    if (image.size() != *expectedPhysicalSize) {
        fail(QStringLiteral("Rendered image is %1x%2; expected %3x%4 pixels")
                 .arg(image.width())
                 .arg(image.height())
                 .arg(expectedPhysicalSize->width())
                 .arg(expectedPhysicalSize->height()));
        return;
    }

    const QFileInfo outputInfo(m_outputPath);
    if (!outputInfo.absoluteDir().mkpath(QStringLiteral("."))) {
        fail(QStringLiteral("Cannot create screenshot directory: %1")
                 .arg(outputInfo.absolutePath()));
        return;
    }
    if (!image.save(m_outputPath, "PNG")) {
        fail(QStringLiteral("Cannot write screenshot: %1").arg(m_outputPath));
        return;
    }

    m_finished = true;
    m_timeout.stop();
    emit finished(true, outputInfo.absoluteFilePath());
}

void ScreenshotCapture::fail(const QString &message)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_timeout.stop();
    emit finished(false, message);
}

} // namespace QindaQt::Shell
