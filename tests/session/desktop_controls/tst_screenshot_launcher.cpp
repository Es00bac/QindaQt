// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/screenshot_launcher.h>

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Session::DesktopControls;

namespace {

QString makeExecutableHelper(const QTemporaryDir &directory, const QString &name)
{
    const QString path = directory.path() + QLatin1Char('/') + name;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    file.write(("#!/bin/sh\nprintf ok > " + directory.filePath(QStringLiteral("started"))
                + "\n")
                   .toUtf8());
    file.close();
    if (!QFile::setPermissions(path, QFileDevice::ExeUser | QFileDevice::ReadUser
                                          | QFileDevice::WriteUser)) {
        return {};
    }
    return path;
}

} // namespace

class ScreenshotLauncherTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void launchesResolvedAbsoluteProgramDetached();
    void missingProgramReportsHonestFailure();
    void emptyProgramReportsHonestFailure();
    void bareForeignProgramNameNeverFallsBackToAmbientPath();
};

void ScreenshotLauncherTest::launchesResolvedAbsoluteProgramDetached() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helper = makeExecutableHelper(directory, QStringLiteral("fake-spectacle"));
    QVERIFY(QFileInfo(helper).isExecutable());

    ScreenshotLauncher launcher(helper, QStringList{QStringLiteral("-b"), QStringLiteral("-r")});
    QSignalSpy started(&launcher, &ScreenshotLauncher::launchStarted);
    QSignalSpy failed(&launcher, &ScreenshotLauncher::launchFailed);
    launcher.launch();

    QCOMPARE(failed.count(), 0);
    QCOMPARE(started.count(), 1);
    const qint64 processId = started.at(0).at(0).toLongLong();
    QVERIFY(processId > 0);
    // The helper writes a marker file, proving the detached child really ran
    // with the launcher's own lifetime.
    const QString marker = directory.filePath(QStringLiteral("started"));
    QTRY_VERIFY(QFileInfo::exists(marker));
}

void ScreenshotLauncherTest::missingProgramReportsHonestFailure() {
    ScreenshotLauncher launcher(QStringLiteral("/nonexistent/screenshot-tool"), {});
    QSignalSpy started(&launcher, &ScreenshotLauncher::launchStarted);
    QSignalSpy failed(&launcher, &ScreenshotLauncher::launchFailed);
    launcher.launch();

    QCOMPARE(started.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(),
             QStringLiteral("screenshot-program-unavailable"));
}

void ScreenshotLauncherTest::emptyProgramReportsHonestFailure() {
    ScreenshotLauncher launcher(QString{}, {});
    QSignalSpy failed(&launcher, &ScreenshotLauncher::launchFailed);
    launcher.launch();

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(),
             QStringLiteral("screenshot-program-unavailable"));
}

void ScreenshotLauncherTest::bareForeignProgramNameNeverFallsBackToAmbientPath() {
    // A bare name that exists only somewhere in ambient PATH is still allowed
    // through findExecutable (the installed sibling wins first); the guard
    // under test is that a nonexistent bare name never fabricates success.
    ScreenshotLauncher launcher(QStringLiteral("definitely-not-a-real-screenshot-tool-4711"), {});
    QSignalSpy started(&launcher, &ScreenshotLauncher::launchStarted);
    QSignalSpy failed(&launcher, &ScreenshotLauncher::launchFailed);
    launcher.launch();

    QCOMPARE(started.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(),
             QStringLiteral("screenshot-program-unavailable"));
}

QTEST_MAIN(ScreenshotLauncherTest)
#include "tst_screenshot_launcher.moc"
