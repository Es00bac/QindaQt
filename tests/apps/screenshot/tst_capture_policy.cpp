// SPDX-License-Identifier: GPL-3.0-or-later
// Capture-request builder, command line and file policy: the pure rules the
// tool, its shortcuts and its scripts share (ADR-0289).
#include "capture_file_policy.h"
#include "capture_request.h"
#include "command_line.h"
#include "result_notifier.h"

#include <qindaqt/services/screenshot_preferences/file_name_pattern.h>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Screenshot;
using namespace QindaQt::Services::ScreenshotPreferences;

namespace {

QImage sample(QColor color = Qt::red)
{
    QImage image(8, 6, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    return image;
}

Invocation parse(QStringList arguments)
{
    arguments.prepend(QStringLiteral("qindaqt-screenshot"));
    return parseInvocation(arguments);
}

} // namespace

class CapturePolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void everyModeBuildsItsKWinCall();
    void modeIdsRoundTrip();
    void commandLineSelectsOneFlow();
    void commandLineRejectsAmbiguousOrUnsafeInput();
    void fileNamePatternExpandsAndRefusesUnsafeNames();
    void folderResolutionFallsBackToTheDefault();
    void uniquePathNeverReusesAName();
    void defaultSaveNeverOverwrites();
    void confirmedSaveWritesTheChosenFormat();
    void notificationsCarryTheDocumentedActions();
};

void CapturePolicyTest::everyModeBuildsItsKWinCall()
{
    CaptureOptions options;
    options.includePointer = true;
    const KWinCaptureCall region = kwinCallFor(options);
    QCOMPARE(region.method, QStringLiteral("CaptureWorkspace"));
    QVERIFY(region.leadingArguments.isEmpty());
    QCOMPARE(region.options.value(QStringLiteral("native-resolution")).toBool(), true);
    QCOMPARE(region.options.value(QStringLiteral("include-cursor")).toBool(), true);
    QCOMPARE(region.options.value(QStringLiteral("hide-caller-windows")).toBool(), true);
    // Screen captures never carry window-only options.
    QVERIFY(!region.options.contains(QStringLiteral("include-decoration")));

    options.mode = CaptureMode::AllScreens;
    options.includePointer = false;
    QCOMPARE(kwinCallFor(options).method, QStringLiteral("CaptureWorkspace"));
    QCOMPARE(kwinCallFor(options).options.value(QStringLiteral("include-cursor")).toBool(), false);
    options.mode = CaptureMode::CurrentScreen;
    QCOMPARE(kwinCallFor(options).method, QStringLiteral("CaptureActiveScreen"));

    options.mode = CaptureMode::ActiveWindow;
    options.includeDecorations = false;
    const KWinCaptureCall active = kwinCallFor(options);
    QCOMPARE(active.method, QStringLiteral("CaptureActiveWindow"));
    QCOMPARE(active.options.value(QStringLiteral("include-decoration")).toBool(), false);
    QCOMPARE(active.options.value(QStringLiteral("include-shadow")).toBool(), false);

    options.mode = CaptureMode::WindowUnderPointer;
    options.includeDecorations = true;
    const KWinCaptureCall picked = kwinCallFor(options);
    QCOMPARE(picked.method, QStringLiteral("CaptureInteractive"));
    QCOMPARE(picked.leadingArguments, QVariantList{QVariant::fromValue(uint(0))});
    QCOMPARE(picked.options.value(QStringLiteral("include-decoration")).toBool(), true);
    // The user picks with a click, so the call waits on them, not on KWin.
    QVERIFY(picked.timeoutMilliseconds > active.timeoutMilliseconds);
}

void CapturePolicyTest::modeIdsRoundTrip()
{
    for (const auto mode : {CaptureMode::Region, CaptureMode::AllScreens, CaptureMode::CurrentScreen,
                            CaptureMode::ActiveWindow, CaptureMode::WindowUnderPointer}) {
        QCOMPARE(captureModeFromId(captureModeId(mode)), std::optional<CaptureMode>(mode));
    }
    QVERIFY(!captureModeFromId(u"everything").has_value());
    QCOMPARE(allowedDelays(), (QList<int>{0, 3, 5, 10}));
    QVERIFY(isWindowMode(CaptureMode::ActiveWindow));
    QVERIFY(!isWindowMode(CaptureMode::Region));
}

void CapturePolicyTest::commandLineSelectsOneFlow()
{
    Invocation window = parse({});
    QVERIFY(window.error.isEmpty());
    QCOMPARE(window.action, Invocation::Action::Window);
    QVERIFY(!window.windowless());

    // The desktop-controls shortcuts launch exactly these (ADR-0289).
    const Invocation region = parse({QStringLiteral("--region")});
    QCOMPARE(region.action, Invocation::Action::Capture);
    QCOMPARE(region.capture.mode, CaptureMode::Region);
    QVERIFY(!region.windowless());
    QCOMPARE(parse({QStringLiteral("--fullscreen")}).capture.mode, CaptureMode::AllScreens);
    QCOMPARE(parse({QStringLiteral("--active")}).capture.mode, CaptureMode::ActiveWindow);
    QCOMPARE(parse({QStringLiteral("--window")}).capture.mode, CaptureMode::WindowUnderPointer);
    QCOMPARE(parse({QStringLiteral("--current-screen")}).capture.mode, CaptureMode::CurrentScreen);
    const Invocation record = parse({QStringLiteral("--record-toggle")});
    QCOMPARE(record.action, Invocation::Action::RecordToggle);
    QVERIFY(record.windowless());

    const Invocation copy = parse({QStringLiteral("-f"), QStringLiteral("--copy")});
    QVERIFY(copy.copy && !copy.save && copy.windowless());

    const Invocation saveDefault = parse({QStringLiteral("--active"), QStringLiteral("--save")});
    QVERIFY(saveDefault.save && saveDefault.savePath.isEmpty());

    const Invocation saveTo = parse({QStringLiteral("--region"), QStringLiteral("--delay"),
                                     QStringLiteral("5"), QStringLiteral("--pointer"),
                                     QStringLiteral("--no-decorations"), QStringLiteral("--save"),
                                     QStringLiteral("/tmp/shot.png"), QStringLiteral("--copy")});
    QVERIFY(saveTo.error.isEmpty());
    QCOMPARE(saveTo.savePath, QStringLiteral("/tmp/shot.png"));
    QCOMPARE(saveTo.capture.delaySeconds, 5);
    QVERIFY(saveTo.capture.includePointer);
    QVERIFY(!saveTo.capture.includeDecorations);
    QVERIFY(saveTo.copy && saveTo.save);

    const Invocation help = parse({QStringLiteral("--help")});
    QVERIFY(help.handled);
    QVERIFY(help.handledText.contains(QStringLiteral("--record-toggle")));
}

void CapturePolicyTest::commandLineRejectsAmbiguousOrUnsafeInput()
{
    QVERIFY(!parse({QStringLiteral("--region"), QStringLiteral("--fullscreen")}).error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--record-toggle"), QStringLiteral("--region")}).error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--record-toggle"), QStringLiteral("--copy")}).error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--copy")}).error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--region"), QStringLiteral("stray.png")}).error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--region"), QStringLiteral("--save"), QStringLiteral("a.png"),
                    QStringLiteral("b.png")})
                 .error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--region"), QStringLiteral("--delay"), QStringLiteral("61")})
                 .error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--region"), QStringLiteral("--delay"), QStringLiteral("soon")})
                 .error.isEmpty());
    QVERIFY(!parse({QStringLiteral("--no-such-flag")}).error.isEmpty());
}

void CapturePolicyTest::fileNamePatternExpandsAndRefusesUnsafeNames()
{
    const QDateTime when(QDate(2026, 9, 28), QTime(21, 5, 9));
    QCOMPARE(expandFileNamePattern(QString::fromLatin1(kDefaultFileNamePattern), when,
                                   QStringLiteral("region")),
             QStringLiteral("Screenshot_2026-09-28_21-05-09.png"));
    QCOMPARE(expandFileNamePattern(QStringLiteral("{mode} at {time}"), when,
                                   QStringLiteral("active-window")),
             QStringLiteral("active-window at 21-05-09.png"));
    for (const QString &unsafe : {QString(), QStringLiteral("  "), QStringLiteral("../x"),
                                  QStringLiteral("a/b"), QStringLiteral(".."), QString(200, u'x')}) {
        QVERIFY2(!isValidFileNamePattern(unsafe), qPrintable(unsafe));
        // An invalid stored pattern falls back to the default name.
        QCOMPARE(expandFileNamePattern(unsafe, when, QStringLiteral("region")),
                 QStringLiteral("Screenshot_2026-09-28_21-05-09.png"));
    }
}

void CapturePolicyTest::folderResolutionFallsBackToTheDefault()
{
    const QString fallback = defaultScreenshotFolder();
    QVERIFY(fallback.endsWith(QStringLiteral("/Screenshots")));
    QCOMPARE(resolveScreenshotFolder(QString()), fallback);
    QCOMPARE(resolveScreenshotFolder(QStringLiteral("relative/dir")), fallback);
    QCOMPARE(resolveScreenshotFolder(QStringLiteral("/srv/shots/")), QStringLiteral("/srv/shots"));
    QCOMPARE(resolveScreenshotFolder(QStringLiteral("~/Shots")), QDir::home().filePath(QStringLiteral("Shots")));
}

void CapturePolicyTest::uniquePathNeverReusesAName()
{
    QSet<QString> taken{QStringLiteral("/d/Shot.png"), QStringLiteral("/d/Shot-2.png")};
    const PathExists exists = [&taken](const QString &path) { return taken.contains(path); };
    QCOMPARE(uniquePath(QStringLiteral("/d"), QStringLiteral("Other.png"), exists),
             QStringLiteral("/d/Other.png"));
    QCOMPARE(uniquePath(QStringLiteral("/d"), QStringLiteral("Shot.png"), exists),
             QStringLiteral("/d/Shot-3.png"));
    const PathExists everything = [](const QString &) { return true; };
    QVERIFY(uniquePath(QStringLiteral("/d"), QStringLiteral("Shot.png"), everything).isEmpty());
}

void CapturePolicyTest::defaultSaveNeverOverwrites()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString folder = root.filePath(QStringLiteral("Pictures/Screenshots"));
    const SaveResult first = saveWithoutOverwriting(sample(Qt::red), folder, QStringLiteral("Shot.png"));
    QVERIFY2(first.ok(), qPrintable(first.error));
    QCOMPARE(first.path, QDir(folder).filePath(QStringLiteral("Shot.png")));
    const SaveResult second = saveWithoutOverwriting(sample(Qt::blue), folder, QStringLiteral("Shot.png"));
    QVERIFY2(second.ok(), qPrintable(second.error));
    QCOMPARE(second.path, QDir(folder).filePath(QStringLiteral("Shot-2.png")));
    // The first file still holds the first image.
    QCOMPARE(QImage(first.path).pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(QImage(second.path).pixelColor(0, 0), QColor(Qt::blue));
    QVERIFY(!saveWithoutOverwriting(QImage(), folder, QStringLiteral("Shot.png")).ok());
}

void CapturePolicyTest::confirmedSaveWritesTheChosenFormat()
{
    QTemporaryDir root;
    const QString path = root.filePath(QStringLiteral("chosen.jpg"));
    const SaveResult saved = saveToConfirmedPath(sample(Qt::green), path);
    QVERIFY2(saved.ok(), qPrintable(saved.error));
    QImageReader reader(path);
    QCOMPARE(reader.format(), QByteArray("jpeg"));
    QVERIFY(!saveToConfirmedPath(sample(), QString()).ok());
}

void CapturePolicyTest::notificationsCarryTheDocumentedActions()
{
    // AGENT-CONTRACT: these keys are what ActionInvoked carries back.
    QCOMPARE(ResultNotifier::actionsFor(ResultKind::ScreenshotSaved),
             (QStringList{QStringLiteral("open"), QStringLiteral("Open"), QStringLiteral("copy"),
                          QStringLiteral("Copy"), QStringLiteral("folder"), QStringLiteral("Show in folder")}));
    QCOMPARE(ResultNotifier::actionsFor(ResultKind::RecordingSaved),
             (QStringList{QStringLiteral("open"), QStringLiteral("Open"), QStringLiteral("folder"),
                          QStringLiteral("Show in folder"), QStringLiteral("copy-path"), QStringLiteral("Copy path")}));
    QCOMPARE(ResultNotifier::actionsFor(ResultKind::RecordUnavailable),
             (QStringList{QStringLiteral("settings"), QStringLiteral("Open Streaming settings")}));
    QVERIFY(ResultNotifier::actionsFor(ResultKind::RecordingStarted).isEmpty());
    QCOMPARE(ResultNotifier::bodyFor(ResultKind::ScreenshotSaved, QStringLiteral("/a/b/Shot.png"), {}),
             QStringLiteral("Shot.png"));
    QVERIFY(!ResultNotifier::bodyFor(ResultKind::RecordingSaved, {}, {}).isEmpty());

    // Without a bus the post fails loudly and holds nothing open.
    ResultNotifier notifier(QDBusConnection(QStringLiteral("screenshot-policy-no-bus")));
    QVERIFY(!notifier.start());
    QSignalSpy failed(&notifier, &ResultNotifier::notificationFailed);
    notifier.notify(ResultKind::ScreenshotSaved, QStringLiteral("/a/b/Shot.png"));
    QCOMPARE(failed.count(), 1);
    QVERIFY(!notifier.hasLiveNotifications());
}

QTEST_GUILESS_MAIN(CapturePolicyTest)
#include "tst_capture_policy.moc"
