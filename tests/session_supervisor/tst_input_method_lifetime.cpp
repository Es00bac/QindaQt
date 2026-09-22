// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session_supervisor/session_process_supervisor.h"
#include <QtTest>
#include <csignal>
using namespace QindaQt::SessionSupervisor;

// The session sets QT_IM_MODULE/GTK_IM_MODULE/XMODIFIERS when an input method
// daemon is installed. A toolkit reads those once and builds its input context
// at startup, so an application launched while the daemon is down has no input
// method for its whole life and never retries -- dictation, the on-screen
// keyboard and complex-script input all fail, silently, for that window. The
// daemon therefore has to be a child of this session: a systemd user unit
// cannot do it, because QindaQt never activates graphical-session.target and
// the user manager outlives a logout, so default.target is not reached again.
class InputMethodLifetimeTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void daemonRunsForTheSessionAndIsRestarted()
    {
        qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", "80000");
        SessionProcessOptions options;
        options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.inputMethodDaemonExecutable =
            QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
        options.powerDevilExecutable.clear();
        options.globalShortcutDaemonExecutable.clear();
        options.networkSecretAgentExecutable.clear();
        options.desktopControlsExecutable.clear();
        options.polkitAgentExecutable.clear();
        options.welcomeExecutable.clear();
        options.profileId = QStringLiteral("test-hold-shell");
        options.compositorProcessId = 42424;
        SessionProcessSupervisor supervisor(options);
        QString error;
        QVERIFY2(supervisor.start(&error), qPrintable(error));
        QTRY_VERIFY(supervisor.inputMethodDaemonProcessId() > 1);

        // It must come back if it dies mid-session, or every application
        // started afterwards comes up without an input context.
        const auto initial = supervisor.inputMethodDaemonProcessId();
        QCOMPARE(::kill(static_cast<pid_t>(initial), SIGTERM), 0);
        QTRY_VERIFY(supervisor.inputMethodDaemonProcessId() > 1
                    && supervisor.inputMethodDaemonProcessId() != initial);

        // Teardown is asynchronous; the daemon must not outlive the session.
        supervisor.stop();
        QTRY_COMPARE(supervisor.inputMethodDaemonProcessId(), qint64(0));
    }

    void anAbsentDaemonNeverBlocksLogin()
    {
        SessionProcessOptions options;
        options.notificationHostExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
        options.inputMethodDaemonExecutable =
            QStringLiteral("/nonexistent/ibus-daemon");
        options.powerDevilExecutable.clear();
        options.globalShortcutDaemonExecutable.clear();
        options.networkSecretAgentExecutable.clear();
        options.desktopControlsExecutable.clear();
        options.polkitAgentExecutable.clear();
        options.welcomeExecutable.clear();
        options.profileId = QStringLiteral("test-hold-shell");
        options.compositorProcessId = 42424;
        SessionProcessSupervisor supervisor(options);
        QString error;
        QVERIFY2(supervisor.start(&error), qPrintable(error));
        QCOMPARE(supervisor.inputMethodDaemonProcessId(), qint64(0));
        supervisor.stop();
    }
};

QTEST_MAIN(InputMethodLifetimeTest)
#include "tst_input_method_lifetime.moc"
