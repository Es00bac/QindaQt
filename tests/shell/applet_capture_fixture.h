// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDir>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest/QTest>

// Optional captures of the real popup in the deterministic fake-service journey.
// No host services, window contents, or user settings enter these artifacts.
inline bool captureApplet(QQuickItem *content, const QString &name)
{
    const auto directory = qEnvironmentVariable("QINDAQT_APPLET_CAPTURE_DIR");
    if (directory.isEmpty()) return true;
    if (!content || !content->window() || !QDir().mkpath(directory)) return false;
    QTest::qWait(80);
    return content->window()->grabWindow().save(QDir(directory).filePath(name + ".png"));
}
