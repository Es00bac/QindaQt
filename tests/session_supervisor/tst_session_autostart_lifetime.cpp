// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session_supervisor/session_process_supervisor.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include <cerrno>
#include <csignal>

using namespace QindaQt::SessionSupervisor;

namespace {
void writeFile(const QString &path, const QString &text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream(&file) << text;
}

QStringList markerLines(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

SessionProcessOptions privateOptions()
{
    SessionProcessOptions options;
    options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.networkSecretAgentExecutable.clear();
    options.desktopControlsExecutable.clear();
    options.polkitAgentExecutable.clear();
    options.powerDevilExecutable.clear();
    options.globalShortcutDaemonExecutable.clear();
    options.inputMethodDaemonExecutable.clear();
    options.xembedTrayProxyExecutable.clear();
    options.welcomeExecutable.clear();
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42424;
    return options;
}
}

class SessionAutostartLifetimeTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void launchesOnceDespiteShellRestartAndStopsOwnedChild();
    void emptyRootsNeverScanAmbientAutostart();
    void stopBeforeFirstEventLoopTurnNeverLaunches();
};

void SessionAutostartLifetimeTest::launchesOnceDespiteShellRestartAndStopsOwnedChild()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString user = root.filePath(QStringLiteral("user/autostart"));
    const QString system = root.filePath(QStringLiteral("system/autostart"));
    QVERIFY(QDir().mkpath(user));
    QVERIFY(QDir().mkpath(system));
    const QString marker = root.filePath(QStringLiteral("started"));
    const QString denied = root.filePath(QStringLiteral("denied"));
    const QString helper = QStringLiteral(QINDAQT_SESSION_AUTOSTART_CHILD_HELPER);
    writeFile(QDir(user).filePath(QStringLiteral("enabled.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=Enabled\nOnlyShowIn=QindaQt;\nExec=%1 %2\n")
                  .arg(helper, marker));
    writeFile(QDir(user).filePath(QStringLiteral("disabled.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=Disabled\nExec=%1 %2\nHidden=true\n")
                  .arg(helper, denied));
    writeFile(QDir(system).filePath(QStringLiteral("foreign.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=Foreign\nExec=%1 %2\nOnlyShowIn=GNOME;\n")
                  .arg(helper, denied));

    auto options = privateOptions();
    options.autostart.userDirectory = user;
    options.autostart.systemDirectories = {system};
    options.autostart.desktops = {QStringLiteral("QindaQt")};
    SessionProcessSupervisor supervisor(options);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(markerLines(marker).size(), 1, 5000);
    QVERIFY(!QFile::exists(denied));
    const qint64 childPid = markerLines(marker).constFirst().toLongLong();
    QVERIFY(childPid > 1);
    const qint64 firstShellPid = supervisor.shellProcessId();
    QVERIFY(firstShellPid > 1);
    QCOMPARE(::kill(static_cast<pid_t>(firstShellPid), SIGTERM), 0);
    QTRY_VERIFY_WITH_TIMEOUT(supervisor.shellProcessId() > 1
                             && supervisor.shellProcessId() != firstShellPid, 8000);
    QCOMPARE(markerLines(marker).size(), 1);
    supervisor.stop();
    QCOMPARE(::kill(static_cast<pid_t>(childPid), 0), -1);
    QCOMPARE(errno, ESRCH);
}

void SessionAutostartLifetimeTest::emptyRootsNeverScanAmbientAutostart()
{
    auto options = privateOptions();
    SessionProcessSupervisor supervisor(options);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QVERIFY(supervisor.isRunning());
    supervisor.stop();
}

void SessionAutostartLifetimeTest::stopBeforeFirstEventLoopTurnNeverLaunches()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString user = root.filePath(QStringLiteral("user/autostart"));
    QVERIFY(QDir().mkpath(user));
    const QString marker = root.filePath(QStringLiteral("too-late"));
    writeFile(QDir(user).filePath(QStringLiteral("late.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=Late\n"
                             "Exec=%1 %2\n")
                  .arg(QStringLiteral(QINDAQT_SESSION_AUTOSTART_CHILD_HELPER),
                       marker));
    auto options = privateOptions();
    options.autostart.userDirectory = user;
    options.autostart.desktops = {QStringLiteral("QindaQt")};
    SessionProcessSupervisor supervisor(options);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    supervisor.stop();
    QCoreApplication::processEvents();
    QTest::qWait(50);
    QVERIFY(!QFile::exists(marker));
}

QTEST_MAIN(SessionAutostartLifetimeTest)
#include "tst_session_autostart_lifetime.moc"
