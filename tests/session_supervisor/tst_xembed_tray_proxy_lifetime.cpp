// SPDX-License-Identifier: GPL-3.0-or-later

// The XEmbed tray proxy (ADR-0229) is a supervised optional child, not a
// systemd unit. Its unit is `WantedBy=graphical-session.target` and QindaQt
// never activates that target, so systemd started it never: the proxy shipped
// and nothing ran it, leaving `_NET_SYSTEM_TRAY_S0` unowned and every
// Wine/Proton/Steam tray icon with nowhere to dock. These rows pin the
// replacement: the supervisor starts it, restarts it once, stops it with the
// session, and treats it as genuinely optional.

#include "qindaqt/session_supervisor/session_process_supervisor.h"

#include <QSignalSpy>
#include <QtTest>

#include <cerrno>
#include <csignal>

using namespace QindaQt::SessionSupervisor;

namespace {

[[nodiscard]] SessionProcessOptions sessionHoldingTheShell()
{
    SessionProcessOptions options;
    options.notificationHostExecutable =
        QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.shellExecutable = QStringLiteral(QINDAQT_SESSION_TOKEN_CHILD_HELPER);
    options.networkSecretAgentExecutable.clear();
    options.desktopControlsExecutable.clear();
    options.polkitAgentExecutable.clear();
    options.powerDevilExecutable.clear();
    options.globalShortcutDaemonExecutable.clear();
    options.welcomeExecutable.clear();
    options.profileId = QStringLiteral("test-hold-shell");
    options.compositorProcessId = 42424;
    return options;
}

} // namespace

class XembedTrayProxyLifetimeTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void theProxyStartsRestartsOnceAndStopsWithTheSession();
    void anAbnormalSessionEndLeavesNoSelectionOwnerBehind();
    void anAbsentProxyNeverPreventsLogin();
};

void XembedTrayProxyLifetimeTest::
    theProxyStartsRestartsOnceAndStopsWithTheSession()
{
    qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", "80000");
    SessionProcessOptions options = sessionHoldingTheShell();
    options.xembedTrayProxyExecutable =
        QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);

    SessionProcessSupervisor supervisor(options);
    QSignalSpy finished(&supervisor, &SessionProcessSupervisor::finished);
    QSignalSpy stopRequested(&supervisor,
                             &SessionProcessSupervisor::childStopRequested);
    QString error;

    // Twice, because a logout must leave the next session able to start it
    // again with a fresh restart budget.
    for (int session = 0; session != 2; ++session) {
        QVERIFY2(supervisor.start(&error), qPrintable(error));
        QTRY_VERIFY(supervisor.xembedTrayProxyProcessId() > 1);

        const auto initial = supervisor.xembedTrayProxyProcessId();
        QCOMPARE(::kill(static_cast<pid_t>(initial), SIGTERM), 0);
        QTRY_VERIFY(supervisor.xembedTrayProxyProcessId() > 1
                    && supervisor.xembedTrayProxyProcessId() != initial);

        // The proxy is never part of readiness: losing it does not end the
        // session, exactly like PowerDevil and the shortcut daemon.
        QVERIFY(supervisor.isRunning());
        QCOMPARE(finished.size(), 0);

        const auto replacement = supervisor.xembedTrayProxyProcessId();
        supervisor.stop();
        QCOMPARE(supervisor.xembedTrayProxyProcessId(), qint64(0));
        // Really gone, not merely forgotten - a surviving proxy would hold the
        // tray selection against the next session's.
        QCOMPARE(::kill(static_cast<pid_t>(replacement), 0), -1);
        QCOMPARE(errno, ESRCH);
    }

    bool sawProxyStop = false;
    for (const auto &emitted : stopRequested) {
        if (emitted.first().toString() == QStringLiteral("xembed-tray-proxy"))
            sawProxyStop = true;
    }
    QVERIFY2(sawProxyStop,
             "the proxy's stop must be announced under its own role");

    qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
}

void XembedTrayProxyLifetimeTest::
    anAbnormalSessionEndLeavesNoSelectionOwnerBehind()
{
    // An orderly stop() cleaned these up; the abnormal path - an essential
    // child dying - did not, so both leaked. Each owns a singleton the NEXT
    // session needs: `_NET_SYSTEM_TRAY_S0` for the proxy, KGlobalAccel's bus
    // name for the shortcut daemon. A survivor makes the next login quietly
    // wrong rather than loudly broken, which is why this row exists.
    qputenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS", "80000");
    SessionProcessOptions options = sessionHoldingTheShell();
    options.xembedTrayProxyExecutable =
        QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);
    options.globalShortcutDaemonExecutable =
        QStringLiteral(QINDAQT_SESSION_PLAIN_CHILD_HELPER);

    SessionProcessSupervisor supervisor(options);
    QSignalSpy finished(&supervisor, &SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QTRY_VERIFY(supervisor.xembedTrayProxyProcessId() > 1);
    QTRY_VERIFY(supervisor.globalShortcutDaemonProcessId() > 1);
    QTRY_VERIFY(supervisor.notificationHostProcessId() > 1);

    const auto proxy = supervisor.xembedTrayProxyProcessId();
    const auto shortcuts = supervisor.globalShortcutDaemonProcessId();

    // Kill the notification host: an essential child, so the session ends.
    QCOMPARE(::kill(static_cast<pid_t>(supervisor.notificationHostProcessId()),
                    SIGKILL),
             0);
    QTRY_COMPARE(finished.size(), 1);

    QCOMPARE(supervisor.xembedTrayProxyProcessId(), qint64(0));
    QCOMPARE(supervisor.globalShortcutDaemonProcessId(), qint64(0));
    QTRY_COMPARE(::kill(static_cast<pid_t>(proxy), 0), -1);
    QCOMPARE(errno, ESRCH);
    QTRY_COMPARE(::kill(static_cast<pid_t>(shortcuts), 0), -1);
    QCOMPARE(errno, ESRCH);

    qunsetenv("QINDAQT_TEST_PLAIN_CHILD_MILLISECONDS");
}

void XembedTrayProxyLifetimeTest::anAbsentProxyNeverPreventsLogin()
{
    SessionProcessOptions options = sessionHoldingTheShell();
    // Empty: a machine where the proxy is not installed.
    options.xembedTrayProxyExecutable.clear();

    SessionProcessSupervisor supervisor(options);
    QSignalSpy finished(&supervisor, &SessionProcessSupervisor::finished);
    QString error;
    QVERIFY2(supervisor.start(&error), qPrintable(error));
    QTRY_VERIFY(supervisor.shellProcessId() > 1);

    QCOMPARE(supervisor.xembedTrayProxyProcessId(), qint64(0));
    QVERIFY(supervisor.isRunning());
    QCOMPARE(finished.size(), 0);
    supervisor.stop();

    // And a configured-but-missing executable is the same story: the default
    // is a bare program name, so a machine without it on PATH must behave
    // like the empty case rather than failing the session.
    SessionProcessOptions missing = sessionHoldingTheShell();
    missing.xembedTrayProxyExecutable =
        QStringLiteral("qindaqt-xembed-tray-proxy-that-is-not-installed");
    SessionProcessSupervisor second(missing);
    QSignalSpy secondFinished(&second, &SessionProcessSupervisor::finished);
    QVERIFY2(second.start(&error), qPrintable(error));
    QTRY_VERIFY(second.shellProcessId() > 1);
    QCOMPARE(second.xembedTrayProxyProcessId(), qint64(0));
    QVERIFY(second.isRunning());
    QCOMPARE(secondFinished.size(), 0);
    second.stop();
}

QTEST_GUILESS_MAIN(XembedTrayProxyLifetimeTest)

#include "tst_xembed_tray_proxy_lifetime.moc"
