// SPDX-License-Identifier: LGPL-3.0-or-later
#include "pe_fixture.h"
#include "model/launch_intent.h"
#include "model/open_with_launcher.h"
#include <qindaqt/windows_runner/executable_open.h>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QtTest>

using namespace QindaQt::WindowsRunner;
using namespace QindaQt::Apps::FileManager;

const QString runtimeHandlerDocument = QStringLiteral("[Desktop Entry]\nType=Application\nName=Windows runner\nExec=/usr/bin/qindaqt-windows-runner %f\n");

struct WindowsOpen final : WindowsExecutableOpenPort {
    bool nativeDefault = false;
    int opens = 0;
    WindowsExecutableOpenRequest received;
    QString openedPath;
    ExecutableOpenDisposition disposition = ExecutableOpenDisposition::Starting;
    bool handlesRequest(const QString &, const WindowsExecutableOpenRequest &request) const override
    {
        return request.mode == ExecutableOpenMode::DefaultHandler ? !nativeDefault
            : request.chosenApplicationId == QStringLiteral("org.qindaqt.WindowsRunner.desktop")
                && request.chosenHandlerContentDigest == QString::fromLatin1(
                    QCryptographicHash::hash(runtimeHandlerDocument.toUtf8(), QCryptographicHash::Sha256).toHex());
    }
    WindowsExecutableOpenResult open(std::unique_ptr<WindowsExecutableFile> file,
        const WindowsExecutableOpenRequest &request) override
    {
        if (!file->stillCurrent()) { return {ExecutableOpenDisposition::Failed, QStringLiteral("changed")}; }
        ++opens; received = request; openedPath = file->canonicalPath();
        return {disposition, disposition == ExecutableOpenDisposition::SetupOpened
            ? QStringLiteral("Choose a Wine or Proton profile for this program.")
            : QStringLiteral("Starting Windows program.")};
    }
};

QindaQt::ApplicationCatalog::ScannedApplication handler(const QString &id)
{
    QindaQt::ApplicationCatalog::ScannedApplication app;
    app.entry.id = id; app.entry.name = QStringLiteral("Chosen handler");
    app.desktopFilePath = QStringLiteral("/private/applications/") + id;
    app.documentText = QStringLiteral("[Desktop Entry]\nType=Application\nName=Chosen handler\nExec=/usr/bin/chosen-editor --literal %f\n");
    if (id == QStringLiteral("org.qindaqt.WindowsRunner.desktop")) { app.documentText = runtimeHandlerDocument; }
    return app;
}

class FileManagerWindowsOpenTests final : public QObject {
    Q_OBJECT
private slots:
    void doubleClickAndOpenWithReachTheSamePeProfileBoundary()
    {
        QTemporaryDir dir; const QString file = dir.filePath(QStringLiteral("my program;$.exe"));
        QVERIFY(writePe(file)); WindowsOpen windows; int nativeDispatches = 0;
        DesktopFileLauncher doubleClick(&windows, [&](const QString &) { ++nativeDispatches; return true; });
        const auto opened = doubleClick.launch(file);
        QVERIFY(opened.ok()); QCOMPARE(opened.progress, LaunchProgress::Starting);
        QCOMPARE(windows.received.mode, ExecutableOpenMode::DefaultHandler);
        QCOMPARE(windows.openedPath, file);
        OpenWithLauncher openWith([&](const QString &, const QStringList &) { ++nativeDispatches; return true; }, {}, &windows);
        const auto chosen = openWith.launch(handler(QStringLiteral("org.qindaqt.WindowsRunner.desktop")), {file});
        QVERIFY(chosen.ok()); QCOMPARE(chosen.progress, LaunchProgress::Starting);
        QCOMPARE(windows.received.mode, ExecutableOpenMode::OpenWith);
        QCOMPARE(windows.received.chosenApplicationId, QStringLiteral("org.qindaqt.WindowsRunner.desktop"));
        QCOMPARE(windows.opens, 2); QCOMPARE(nativeDispatches, 0);
    }
    void renamedTextNeverReachesWindowsLaunch()
    {
        QTemporaryDir dir; const QString file = dir.filePath(QStringLiteral("script.exe"));
        QVERIFY(writePe(file, QByteArrayLiteral("echo shell script"))); WindowsOpen windows;
        int nativeDispatches = 0;
        DesktopFileLauncher doubleClick(&windows, [&](const QString &) { ++nativeDispatches; return true; });
        QVERIFY(!doubleClick.launch(file).ok());
        OpenWithLauncher openWith([&](const QString &, const QStringList &) { ++nativeDispatches; return true; }, {}, &windows);
        QVERIFY(!openWith.launch(handler(QStringLiteral("org.qindaqt.WindowsRunner.desktop")), {file}).ok());
        QCOMPARE(windows.opens, 0); QCOMPARE(nativeDispatches, 0);
    }
    void explicitNativeDefaultAndOpenWithArePreserved()
    {
        QTemporaryDir dir; const QString file = dir.filePath(QStringLiteral("view-as-text.exe"));
        QVERIFY(writePe(file, QByteArrayLiteral("text intentionally chosen for editor")));
        WindowsOpen windows; windows.nativeDefault = true; int nativeDispatches = 0;
        DesktopFileLauncher doubleClick(&windows, [&](const QString &path) {
            ++nativeDispatches; return path == file;
        });
        QVERIFY(doubleClick.launch(file).ok());
        OpenWithLauncher openWith([&](const QString &program, const QStringList &arguments) {
            ++nativeDispatches;
            return program == QStringLiteral("/usr/bin/chosen-editor")
                && arguments == QStringList({QStringLiteral("--literal"), file});
        }, {}, &windows);
        QVERIFY(openWith.launch(handler(QStringLiteral("chosen-editor.desktop")), {file}).ok());
        QCOMPARE(nativeDispatches, 2); QCOMPARE(windows.opens, 0);
    }
    void nativeOverrideOfTheSameHandlerIdDoesNotInheritWindowsRouting()
    {
        QTemporaryDir dir; const QString file = dir.filePath(QStringLiteral("chosen.exe"));
        QVERIFY(writePe(file, QByteArrayLiteral("opened as text"))); WindowsOpen windows;
        auto selected = handler(QStringLiteral("org.qindaqt.WindowsRunner.desktop"));
        selected.documentText = QStringLiteral("[Desktop Entry]\nType=Application\nName=My editor\nExec=/usr/bin/chosen-editor %f\n");
        int native = 0; OpenWithLauncher openWith([&](const QString &program, const QStringList &arguments) {
            ++native; return program == QStringLiteral("/usr/bin/chosen-editor") && arguments == QStringList{file};
        }, {}, &windows);
        QVERIFY(openWith.launch(selected, {file}).ok());
        QCOMPARE(native, 1); QCOMPARE(windows.opens, 0);
    }
    void firstRunSetupHasASeparateProgressState()
    {
        QTemporaryDir dir; const QString file = dir.filePath(QStringLiteral("installer.exe"));
        QVERIFY(writePe(file)); WindowsOpen windows; windows.disposition = ExecutableOpenDisposition::SetupOpened;
        const auto result = DesktopFileLauncher(&windows).launch(file);
        QVERIFY(result.ok()); QCOMPARE(result.progress, LaunchProgress::SetupOpened);
        QVERIFY(!result.diagnostic.isEmpty());
    }
    void uncertainWindowsSubmissionNeverFallsBack()
    {
        QTemporaryDir dir; const QString file = dir.filePath(QStringLiteral("portable.exe"));
        QVERIFY(writePe(file)); WindowsOpen windows;
        windows.disposition = ExecutableOpenDisposition::OutcomeUnknown; int fallback = 0;
        const auto result = DesktopFileLauncher(&windows, [&](const QString &) { ++fallback; return true; }).launch(file);
        QVERIFY(!result.ok()); QCOMPARE(result.progress, LaunchProgress::OutcomeUnknown);
        QCOMPARE(windows.opens, 1); QCOMPARE(fallback, 0);
    }
};
QTEST_GUILESS_MAIN(FileManagerWindowsOpenTests)
#include "tst_file_manager_windows_open.moc"
