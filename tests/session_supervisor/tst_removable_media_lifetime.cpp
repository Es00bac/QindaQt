// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include <QSignalSpy>
#include <QtTest>
#include <cerrno>
#include <csignal>

using namespace QindaQt::SessionSupervisor;
class RemovableMediaLifetimeTest final : public QObject {
    Q_OBJECT
    static SessionProcessOptions options() {
        SessionProcessOptions value;
        value.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        value.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        value.networkSecretAgentExecutable.clear(); value.desktopControlsExecutable.clear();
        value.welcomeExecutable.clear(); value.xembedTrayProxyExecutable.clear();
        value.profileId = QStringLiteral("test-hold-shell"); value.compositorProcessId = 42424;
        return value;
    }
private Q_SLOTS:
    void watcherStartsRestartsAndStopsBeforeNotificationHost() {
        qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", "80000");
        auto value = options(); value.removableMediaExecutable = QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
        SessionProcessSupervisor supervisor(value);
        QSignalSpy stopped(&supervisor, &SessionProcessSupervisor::childStopRequested);
        QString error; QVERIFY2(supervisor.start(&error), qPrintable(error));
        QTRY_VERIFY(supervisor.removableMediaProcessId() > 1);
        const auto first = supervisor.removableMediaProcessId();
        QCOMPARE(::kill(static_cast<pid_t>(first), SIGTERM), 0);
        QTRY_VERIFY(supervisor.removableMediaProcessId() > 1 && supervisor.removableMediaProcessId() != first);
        const auto second = supervisor.removableMediaProcessId(); supervisor.stop();
        QCOMPARE(supervisor.removableMediaProcessId(), qint64(0));
        QCOMPARE(::kill(static_cast<pid_t>(second), 0), -1); QCOMPARE(errno, ESRCH);
        QStringList roles; for (const auto &row : stopped) roles.append(row.constFirst().toString());
        QVERIFY(roles.indexOf(QStringLiteral("removable-media")) >= 0);
        QVERIFY(roles.indexOf(QStringLiteral("removable-media")) < roles.indexOf(QStringLiteral("notification-host")));
        qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
    }
    void abnormalSessionEndStopsWatcher() {
        qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", "80000");
        auto value = options(); value.removableMediaExecutable = QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
        SessionProcessSupervisor supervisor(value); QSignalSpy finished(&supervisor, &SessionProcessSupervisor::finished);
        QString error; QVERIFY2(supervisor.start(&error), qPrintable(error));
        QTRY_VERIFY(supervisor.removableMediaProcessId() > 1);
        const auto watcher = supervisor.removableMediaProcessId();
        QCOMPARE(::kill(static_cast<pid_t>(supervisor.notificationHostProcessId()), SIGKILL), 0);
        QTRY_COMPARE(finished.size(), 1); QCOMPARE(supervisor.removableMediaProcessId(), qint64(0));
        QCOMPARE(::kill(static_cast<pid_t>(watcher), 0), -1); QCOMPARE(errno, ESRCH);
        qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
    }
    void absentWatcherAndPrivateDefaultDoNotPreventLogin() {
        for (const QString &program : {QString{}, QStringLiteral("qindaqt-removable-media-not-installed")}) {
            auto value = options(); value.removableMediaExecutable = program;
            SessionProcessSupervisor supervisor(value); QString error;
            QVERIFY2(supervisor.start(&error), qPrintable(error));
            QVERIFY(supervisor.isRunning()); QCOMPARE(supervisor.removableMediaProcessId(), qint64(0));
            supervisor.stop();
        }
    }
};
QTEST_GUILESS_MAIN(RemovableMediaLifetimeTest)
#include "tst_removable_media_lifetime.moc"
