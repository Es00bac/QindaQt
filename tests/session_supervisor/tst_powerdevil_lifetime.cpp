// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include <QSignalSpy>
#include <QtTest>
#include <cerrno>
#include <csignal>
using namespace QindaQt::SessionSupervisor;

class PowerDevilLifetimeTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void daemonRestartAndLogoutStayInsideTheSession()
    {
        qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", "80000");
        SessionProcessOptions options;
        options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.powerDevilExecutable = QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
        options.networkSecretAgentExecutable.clear();
        options.desktopControlsExecutable.clear();
        options.polkitAgentExecutable.clear();
        options.welcomeExecutable.clear();
        options.profileId = QStringLiteral("test-hold-shell");
        options.compositorProcessId = 42424;
        SessionProcessSupervisor supervisor(options);
        QSignalSpy finished(&supervisor, &SessionProcessSupervisor::finished);
        QString error;
        QVERIFY2(supervisor.start(&error), qPrintable(error));
        QTRY_VERIFY(supervisor.powerDevilProcessId() > 1);
        const auto initial = supervisor.powerDevilProcessId();
        QCOMPARE(::kill(static_cast<pid_t>(initial), SIGTERM), 0);
        QTRY_VERIFY(supervisor.powerDevilProcessId() > 1
                    && supervisor.powerDevilProcessId() != initial);
        QVERIFY(supervisor.isRunning());
        QCOMPARE(finished.size(), 0);
        const auto replacement = supervisor.powerDevilProcessId();
        supervisor.stop();
        QCOMPARE(supervisor.powerDevilProcessId(), qint64(0));
        QCOMPARE(::kill(static_cast<pid_t>(replacement), 0), -1);
        QCOMPARE(errno, ESRCH);
        qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
    }
};
QTEST_GUILESS_MAIN(PowerDevilLifetimeTest)
#include "tst_powerdevil_lifetime.moc"
