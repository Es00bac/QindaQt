#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include <QFile>
#include <QtTest>
#include <csignal>
using namespace QindaQt::SessionSupervisor;

class NightLightLifetimeTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void serviceIsArgumentlessResidentAndRestartsOnce()
    {
        qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", "80000");
        SessionProcessOptions options;
        options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.nightLightExecutable = QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
        options.powerDevilExecutable.clear();
        options.globalShortcutDaemonExecutable.clear();
        options.inputMethodDaemonExecutable.clear();
        options.networkSecretAgentExecutable.clear();
        options.desktopControlsExecutable.clear();
        options.polkitAgentExecutable.clear();
        options.welcomeExecutable.clear();
        options.keyringExecutable.clear();
        options.profileId = QStringLiteral("test-hold-shell");
        options.compositorProcessId = 42424;
        SessionProcessSupervisor supervisor(options);
        QString error;
        QVERIFY2(supervisor.start(&error), qPrintable(error));
        QTRY_VERIFY(supervisor.nightLightProcessId() > 1);

        QFile commandLine(QStringLiteral("/proc/%1/cmdline")
                              .arg(supervisor.nightLightProcessId()));
        QVERIFY(commandLine.open(QIODevice::ReadOnly));
        const QStringList arguments = QString::fromLocal8Bit(commandLine.readAll())
                                          .split(QLatin1Char('\0'), Qt::SkipEmptyParts);
        QCOMPARE(arguments.size(), 1);

        const auto initial = supervisor.nightLightProcessId();
        QCOMPARE(::kill(static_cast<pid_t>(initial), SIGTERM), 0);
        QTRY_VERIFY(supervisor.nightLightProcessId() > 1 &&
                    supervisor.nightLightProcessId() != initial);
        QCOMPARE(supervisor.nightLightRestartCount(), 1);
        const auto replacement = supervisor.nightLightProcessId();
        supervisor.stop();
        QTRY_COMPARE(supervisor.nightLightProcessId(), qint64(0));
        QCOMPARE(::kill(static_cast<pid_t>(replacement), 0), -1);
        qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
    }

    void missingServiceDoesNotBlockLogin()
    {
        SessionProcessOptions options;
        options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.nightLightExecutable = QStringLiteral("/nonexistent/qindaqt-night-light-service");
        options.powerDevilExecutable.clear();
        options.globalShortcutDaemonExecutable.clear();
        options.inputMethodDaemonExecutable.clear();
        options.networkSecretAgentExecutable.clear();
        options.desktopControlsExecutable.clear();
        options.polkitAgentExecutable.clear();
        options.welcomeExecutable.clear();
        options.keyringExecutable.clear();
        options.profileId = QStringLiteral("test-hold-shell");
        options.compositorProcessId = 42424;
        SessionProcessSupervisor supervisor(options);
        QString error;
        QVERIFY2(supervisor.start(&error), qPrintable(error));
        QCOMPARE(supervisor.nightLightProcessId(), qint64(0));
        supervisor.stop();
    }
};
QTEST_MAIN(NightLightLifetimeTest)
#include "tst_night_light_lifetime.moc"
