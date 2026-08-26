// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include "qindaqt/session_supervisor/tokenized_process_launcher.h"

#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

#include <utility>

using namespace QindaQt;

class SessionProcessSupervisorTests final : public QObject {
    Q_OBJECT

private slots:
    void launcherPassesASecretOnlyThroughTheDescriptor();
    void supervisorStartsBothChildrenAndCouplesTheirLifetime();
    void supervisorRollsBackWhenTheSecondChildCannotStart();
};

void SessionProcessSupervisorTests::
    launcherPassesASecretOnlyThroughTheDescriptor()
{
    const auto token =
        Services::NotificationPresentation::PresentationAccessToken::generate();
    const QString secret = token.toHex();
    QProcess child;
    QString error;
    QVERIFY2(SessionSupervisor::TokenizedProcessLauncher::start(
                 child, QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER),
                 {QStringLiteral("--quick-exit")}, token, &error),
             qPrintable(error));
    QVERIFY(child.waitForFinished(5'000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
    QVERIFY(child.readAllStandardOutput().contains("token-channel-ok host"));
    QVERIFY(!child.arguments().join(QLatin1Char(' ')).contains(secret));
}

void SessionProcessSupervisorTests::
    supervisorStartsBothChildrenAndCouplesTheirLifetime()
{
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable =
        QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.profileId = QStringLiteral("test-shell-role");
    options.themeId = QStringLiteral("test-theme");
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QSignalSpy finished(&supervisor,
                        &SessionSupervisor::SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QVERIFY(supervisor.isRunning());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5'000);
    QCOMPARE(finished.constFirst().at(0).toInt(), 1);
    QVERIFY(!supervisor.isRunning());
}

void SessionProcessSupervisorTests::
    supervisorRollsBackWhenTheSecondChildCannotStart()
{
    SessionSupervisor::SessionProcessOptions options;
    options.notificationHostExecutable =
        QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral("/definitely/missing/qindaqt-shell");
    SessionSupervisor::SessionProcessSupervisor supervisor(std::move(options));
    QString error;
    QVERIFY(!supervisor.start(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!supervisor.isRunning());
}

QTEST_GUILESS_MAIN(SessionProcessSupervisorTests)

#include "tst_session_process_supervisor.moc"
