// SPDX-License-Identifier: GPL-3.0-or-later
#include "screenshotcapture.h"
#include "capturegeometry.h"
#include <limits>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QTest>
using QindaQt::Shell::ScreenshotCapture;

class ScreenshotCaptureErrorTest final : public QObject {
    Q_OBJECT
private slots:
    void validatesNativeGeometry();
    void rejectsChangedLogicalSize();
    void rejectsResizeBeforeReadback();
    void rejectsReadbackBounds_data();
    void rejectsReadbackBounds();
};

void ScreenshotCaptureErrorTest::validatesNativeGeometry()
{
    using QindaQt::Shell::CaptureGeometry::physicalSize;
    QCOMPARE(physicalSize(QSize(1280, 720), 2).value(), QSize(2560, 1440));
    QCOMPARE(physicalSize(QSize(1280, 720), 1.25).value(), QSize(1600, 900));
    QCOMPARE(physicalSize(QSize(1281, 721), 1.5).value(), QSize(1922, 1082));
    QCOMPARE(physicalSize(QSize(8192, 8192), 1).value(), QSize(8192, 8192));
    QVERIFY(!physicalSize(QSize(1280, 720), 0));
    QVERIFY(!physicalSize(QSize(1280, 720), -1));
    QVERIFY(!physicalSize(QSize(1280, 720), std::numeric_limits<qreal>::infinity()));
    QVERIFY(!physicalSize(QSize(1280, 720), std::numeric_limits<qreal>::quiet_NaN()));
    QVERIFY(!physicalSize(QSize(1280, 720), std::numeric_limits<qreal>::max()));
    QVERIFY(!physicalSize(QSize(10000, 1000), 2));
    QVERIFY(!physicalSize(QSize(4097, 4097), 2));
}

void ScreenshotCaptureErrorTest::rejectsChangedLogicalSize()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString output = directory.filePath(QStringLiteral("absent.png"));
    QQuickWindow window;
    window.resize(640, 480);
    ScreenshotCapture capture(output, QSize(641, 480));
    QSignalSpy finished(&capture, &ScreenshotCapture::finished);
    capture.start(window);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("logical")));
    QVERIFY(!QFileInfo::exists(output));
    capture.start(window);
    QCOMPARE(finished.size(), 1);
}

void ScreenshotCaptureErrorTest::rejectsResizeBeforeReadback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString output = directory.filePath(QStringLiteral("absent.png"));
    QQuickWindow window;
    window.resize(640, 480);
    ScreenshotCapture capture(output, window.size());
    QSignalSpy finished(&capture, &ScreenshotCapture::finished);
    capture.start(window);
    QCOMPARE(finished.size(), 0);
    window.resize(641, 480);
    // The frame callback defers readback onto the GUI event loop. A geometry
    // change during that gap must fail before allocating or writing an image.
    QVERIFY(QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection));
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("logical")));
    QVERIFY(!QFileInfo::exists(output));
}

void ScreenshotCaptureErrorTest::rejectsReadbackBounds_data()
{
    QTest::addColumn<QSize>("logicalSize");
    QTest::newRow("empty") << QSize(0, 480);
    QTest::newRow("axis-limit") << QSize(16385, 480);
    QTest::newRow("pixel-budget") << QSize(10000, 10000);
}

void ScreenshotCaptureErrorTest::rejectsReadbackBounds()
{
    QFETCH(QSize, logicalSize);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString output = directory.filePath(QStringLiteral("absent.png"));
    QQuickWindow window;
    window.resize(640, 480);
    ScreenshotCapture capture(output, logicalSize);
    QSignalSpy finished(&capture, &ScreenshotCapture::finished);
    capture.start(window);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("bounded")));
    QVERIFY(!QFileInfo::exists(output));
}

QTEST_MAIN(ScreenshotCaptureErrorTest)
#include "tst_screenshot_capture_errors.moc"
