// SPDX-License-Identifier: GPL-3.0-or-later
// The QindaTK window offscreen, with QT_FATAL_WARNINGS: capture options, a
// finished capture's actions, the region overlay driven by mouse and keys,
// and the record page with no OBS. The port is a fake; no KWin is needed.
#include "capture_flow.h"
#include "capture_result.h"
#include "clipboard_publisher.h"
#include "fake_capture_port.h"
#include "frame_store.h"
#include "record_controller.h"
#include "result_notifier.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Screenshot;

namespace {

class FakeApp final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString resultMessage READ resultMessage NOTIFY resultMessageChanged)
public:
    QString message;
    int settingsRequests = 0;
    [[nodiscard]] QString resultMessage() const { return message; }
    Q_INVOKABLE void openStreamingSettings() { ++settingsRequests; }
    Q_INVOKABLE void openCaptureSettings() { ++settingsRequests; }
    Q_INVOKABLE void openRecording(const QString &) {}
    Q_INVOKABLE void showRecordingInFolder(const QString &) {}
    Q_INVOKABLE void copyRecordingPath(const QString &) {}
Q_SIGNALS:
    void resultMessageChanged();
};

QList<ScreenGeometry> primaryOnly()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    return {{screen->name(), screen->geometry()}};
}

void click(QQuickWindow *window, QQuickItem *item)
{
    QVERIFY(item);
    QVERIFY2(item->isVisible() && item->isEnabled(), qPrintable(item->objectName()));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
}

QQuickWindow *overlayWindow()
{
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (window->title() == QStringLiteral("Select a region to capture") && window->isVisible())
            return qobject_cast<QQuickWindow *>(window);
    }
    return nullptr;
}

} // namespace

class ScreenshotUiTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void captureResultAndActions();
    void regionOverlayByMouseAndKeyboard();
    void recordPageExplainsMissingObs();
    void cleanupTestCase();

private:
    QQuickItem *find(const char *name) const
    {
        return m_window ? m_window->findChild<QQuickItem *>(QString::fromLatin1(name)) : nullptr;
    }

    QTemporaryDir m_folder;
    FrameStore m_frames;
    FakeCapturePort m_port;
    std::unique_ptr<CaptureFlow> m_flow;
    std::unique_ptr<ClipboardPublisher> m_clipboard;
    std::unique_ptr<ResultNotifier> m_notifier;
    std::unique_ptr<CaptureResult> m_result;
    std::unique_ptr<RecordController> m_recorder;
    FakeApp m_app;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QQuickWindow *m_window = nullptr;
};

void ScreenshotUiTest::initTestCase()
{
    QVERIFY(m_folder.isValid());
    m_flow = std::make_unique<CaptureFlow>(m_port, m_frames, &primaryOnly);
    m_flow->setSettleMilliseconds(0);
    m_clipboard = std::make_unique<ClipboardPublisher>(ClipboardPublisher::AdapterFactory());
    m_notifier = std::make_unique<ResultNotifier>(QDBusConnection(QStringLiteral("screenshot-ui-no-bus")));
    const QString folder = m_folder.path();
    m_result = std::make_unique<CaptureResult>(m_frames, *m_clipboard, *m_notifier,
                                               DesktopActions(QDBusConnection(QStringLiteral("screenshot-ui-no-bus"))),
                                               [folder] { return SaveTarget{folder, QStringLiteral("Shot_{mode}")}; });
    m_recorder = std::make_unique<RecordController>(nullptr);
    connect(m_flow.get(), &CaptureFlow::captured, m_result.get(),
            [this](const QImage &image, const QString &mode) { m_result->setImage(image, mode); });

    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_engine->addImageProvider(QStringLiteral("capture"), new CaptureImageProvider(m_frames));
    QQmlContext *context = m_engine->rootContext();
    context->setContextProperty(QStringLiteral("captureFlow"), m_flow.get());
    context->setContextProperty(QStringLiteral("captureResult"), m_result.get());
    context->setContextProperty(QStringLiteral("recorder"), m_recorder.get());
    context->setContextProperty(QStringLiteral("screenshotApp"), &m_app);
    m_engine->load(QUrl::fromLocalFile(QStringLiteral(SCREENSHOT_QML_DIR "/Main.qml")));
    QVERIFY(!m_engine->rootObjects().isEmpty());
    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
    QVERIFY(m_window);
    QVERIFY(!m_window->isVisible()); // ScreenshotApp decides when to show it
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window));
}

void ScreenshotUiTest::captureResultAndActions()
{
    QVERIFY(find("capturePage")->isVisible());
    QVERIFY(!find("resultPage")->isVisible());
    QVERIFY(!find("includeDecorations")->isEnabled()); // region: no window decorations
    m_flow->setMode(QStringLiteral("active-window"));
    QTRY_VERIFY(find("includeDecorations")->isEnabled());
    QCOMPARE(find("modeBox")->property("currentIndex").toInt(), 3);
    QVERIFY(m_window->grabWindow().save(QStringLiteral(SCREENSHOT_ARTIFACTS_DIR "/capture-page.png")));

    click(m_window, find("takeScreenshot"));
    QTRY_COMPARE(m_port.calls.size(), 1);
    QCOMPARE(m_port.calls.last().method, QStringLiteral("CaptureActiveWindow"));
    QImage shot(320, 200, QImage::Format_ARGB32_Premultiplied);
    shot.fill(QColor(40, 120, 200));
    m_port.answerImage(shot);
    QTRY_VERIFY(find("resultPage")->isVisible());
    QVERIFY(!find("capturePage")->isVisible());
    QTRY_VERIFY(find("resultImage")->property("status").toInt() == 1); // Image.Ready
    QVERIFY(find("resultSummary")->property("text").toString().contains(QStringLiteral("320 × 200")));
    QVERIFY(m_window->grabWindow().save(QStringLiteral(SCREENSHOT_ARTIFACTS_DIR "/result-page.png")));

    click(m_window, find("saveButton"));
    const QString saved = QDir(m_folder.path()).filePath(QStringLiteral("Shot_active-window.png"));
    QTRY_COMPARE(m_result->savedPath(), saved);
    QCOMPARE(QImage(saved).size(), QSize(320, 200));
    QTRY_VERIFY(find("resultNotice")->isVisible());
    // Saving again never replaces the first file.
    click(m_window, find("saveButton"));
    QTRY_COMPARE(m_result->savedPath(), QDir(m_folder.path()).filePath(QStringLiteral("Shot_active-window-2.png")));
    QVERIFY(QFileInfo::exists(saved));

    click(m_window, find("discardButton"));
    QTRY_VERIFY(find("capturePage")->isVisible());
}

void ScreenshotUiTest::regionOverlayByMouseAndKeyboard()
{
    m_flow->setMode(QStringLiteral("region"));
    const QRect screen = QGuiApplication::primaryScreen()->geometry();
    QImage desktop(screen.size(), QImage::Format_ARGB32_Premultiplied);
    desktop.fill(Qt::darkGreen);

    // Escape cancels without a result.
    QSignalSpy cancelled(m_flow.get(), &CaptureFlow::cancelled);
    QVERIFY(m_flow->start());
    QTRY_COMPARE(m_port.calls.size(), 2);
    m_port.answerImage(desktop);
    QTRY_VERIFY(overlayWindow() != nullptr);
    QQuickWindow *overlay = overlayWindow();
    QVERIFY(QTest::qWaitForWindowExposed(overlay));
    QTest::keyClick(overlay, Qt::Key_Escape);
    QTRY_COMPARE(cancelled.count(), 1);
    QTRY_VERIFY(overlayWindow() == nullptr);
    QVERIFY(!m_result->hasImage());

    // A mouse drag frames 100x60 logical pixels; Alt+Right widens it by one.
    QVERIFY(m_flow->start());
    QTRY_COMPARE(m_port.calls.size(), 3);
    m_port.answerImage(desktop);
    QTRY_VERIFY(overlayWindow() != nullptr);
    overlay = overlayWindow();
    QVERIFY(QTest::qWaitForWindowExposed(overlay));
    QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(40, 30));
    QTest::mouseMove(overlay, QPoint(90, 60));
    QTest::mouseMove(overlay, QPoint(140, 90));
    QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(140, 90));
    QQuickItem *frame = overlay->findChild<QQuickItem *>(QStringLiteral("selectionFrame"));
    QTRY_VERIFY(frame && frame->isVisible());
    QVERIFY(overlay->grabWindow().save(QStringLiteral(SCREENSHOT_ARTIFACTS_DIR "/region-overlay.png")));
    QTest::keyClick(overlay, Qt::Key_Right, Qt::AltModifier);
    QTest::keyClick(overlay, Qt::Key_Return);
    QTRY_VERIFY(m_result->hasImage());
    QCOMPARE(m_result->size(), QSize(101, 60));
    QTRY_VERIFY(overlayWindow() == nullptr);
    QTRY_VERIFY(find("resultPage")->isVisible());
    m_result->clear();
}

void ScreenshotUiTest::recordPageExplainsMissingObs()
{
    m_window->setProperty("currentTab", 1);
    QTRY_VERIFY(find("recordPage")->isVisible());
    QVERIFY(!find("recordToggle")->isEnabled());
    QVERIFY(find("recordUnavailable")->isVisible());
    QVERIFY(!find("lastRecording")->isVisible());
    click(m_window, find("recordOpenSettings"));
    QCOMPARE(m_app.settingsRequests, 1);
    QVERIFY(m_window->grabWindow().save(QStringLiteral(SCREENSHOT_ARTIFACTS_DIR "/record-page.png")));
    m_window->setProperty("currentTab", 0);
}

void ScreenshotUiTest::cleanupTestCase()
{
    m_engine.reset();
}

QTEST_MAIN(ScreenshotUiTest)
#include "tst_screenshot_ui.moc"
