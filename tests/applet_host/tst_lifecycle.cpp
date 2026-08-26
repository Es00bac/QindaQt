// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/host_lifecycle.h"

#include <QTest>

using namespace QindaQt::AppletHost;

namespace {

LifecyclePolicy testPolicy()
{
    return {.baseBackoffMs = 100,
            .maximumBackoffMs = 400,
            .stableRuntimeMs = 1'000,
            .crashLimit = 4};
}

void reachRunning(HostLifecycle *lifecycle, qint64 nowMs)
{
    QVERIFY(lifecycle->requestStart(nowMs).action == LifecycleAction::LaunchProcess);
    QVERIFY(lifecycle->processStarted().action == LifecycleAction::AwaitHandshake);
    QVERIFY(lifecycle->handshakeAccepted(nowMs).accepted);
    QVERIFY(lifecycle->state() == LifecycleState::Running);
}

} // namespace

class LifecycleTest final : public QObject {
    Q_OBJECT

private slots:
    void followsNormalStartAndStop();
    void appliesCappedExponentialBackoff();
    void resetsCrashCountAfterStableRuntime();
    void disablesAfterHandshakeRejection();
    void rejectsInvalidTransitions();
    void rejectsInvalidPolicy();
};

void LifecycleTest::followsNormalStartAndStop()
{
    HostLifecycle lifecycle(testPolicy());
    reachRunning(&lifecycle, 10);

    const LifecycleTransition stop = lifecycle.requestStop();
    QVERIFY(stop.accepted);
    QVERIFY(stop.state == LifecycleState::Stopping);
    QVERIFY(stop.action == LifecycleAction::TerminateProcess);
    const LifecycleTransition exited = lifecycle.processExited(
        ProcessExitCause::UnexpectedCleanExit, 20);
    QVERIFY(exited.accepted);
    QVERIFY(exited.state == LifecycleState::Stopped);
    QCOMPARE(lifecycle.consecutiveCrashes(), 0);
}

void LifecycleTest::appliesCappedExponentialBackoff()
{
    HostLifecycle lifecycle(testPolicy());
    reachRunning(&lifecycle, 0);

    LifecycleTransition failure = lifecycle.processExited(ProcessExitCause::Crash, 10);
    QVERIFY(failure.state == LifecycleState::Backoff);
    QVERIFY(failure.action == LifecycleAction::ScheduleRestart);
    QCOMPARE(failure.retryAtMs, 110);
    QCOMPARE(lifecycle.consecutiveCrashes(), 1);
    QVERIFY(!lifecycle.requestStart(109).accepted);
    QVERIFY(lifecycle.requestStart(110).accepted);
    QVERIFY(lifecycle.processExited(ProcessExitCause::LaunchFailure, 111).accepted);
    QCOMPARE(lifecycle.retryAtMs(), 311);

    QVERIFY(lifecycle.requestStart(311).accepted);
    QVERIFY(lifecycle.processStarted().accepted);
    QVERIFY(lifecycle.processExited(ProcessExitCause::Crash, 312).accepted);
    QCOMPARE(lifecycle.retryAtMs(), 712);

    QVERIFY(lifecycle.requestStart(712).accepted);
    failure = lifecycle.processExited(ProcessExitCause::Crash, 713);
    QVERIFY(failure.state == LifecycleState::Disabled);
    QCOMPARE(lifecycle.consecutiveCrashes(), 4);
    QVERIFY(!lifecycle.requestStart(714).accepted);
    QVERIFY(lifecycle.resetDisabled().accepted);
    QVERIFY(lifecycle.state() == LifecycleState::Stopped);
}

void LifecycleTest::resetsCrashCountAfterStableRuntime()
{
    HostLifecycle lifecycle(testPolicy());
    reachRunning(&lifecycle, 0);
    QVERIFY(lifecycle.processExited(ProcessExitCause::Crash, 10).accepted);
    QVERIFY(lifecycle.requestStart(110).accepted);
    QVERIFY(lifecycle.processStarted().accepted);
    QVERIFY(lifecycle.handshakeAccepted(120).accepted);

    const LifecycleTransition failure = lifecycle.processExited(ProcessExitCause::Crash, 1'120);
    QCOMPARE(lifecycle.consecutiveCrashes(), 1);
    QCOMPARE(failure.retryAtMs, 1'220);
}

void LifecycleTest::disablesAfterHandshakeRejection()
{
    HostLifecycle lifecycle(testPolicy());
    QVERIFY(lifecycle.requestStart(0).accepted);
    QVERIFY(lifecycle.processStarted().accepted);
    const LifecycleTransition rejected = lifecycle.handshakeRejected(
        QStringLiteral("Protocol authentication failed"));
    QVERIFY(rejected.state == LifecycleState::Stopping);
    QVERIFY(rejected.action == LifecycleAction::TerminateProcess);

    const LifecycleTransition exited = lifecycle.processExited(ProcessExitCause::Crash, 1);
    QVERIFY(exited.state == LifecycleState::Disabled);
    QVERIFY(exited.message.contains(QStringLiteral("authentication")));
}

void LifecycleTest::rejectsInvalidTransitions()
{
    HostLifecycle lifecycle(testPolicy());
    QVERIFY(!lifecycle.processStarted().accepted);
    QVERIFY(!lifecycle.handshakeAccepted(0).accepted);
    QVERIFY(!lifecycle.processExited(ProcessExitCause::Crash, 0).accepted);
    QVERIFY(!lifecycle.requestStart(-1).accepted);
}

void LifecycleTest::rejectsInvalidPolicy()
{
    LifecyclePolicy invalid = testPolicy();
    invalid.crashLimit = 0;
    HostLifecycle lifecycle(invalid);
    QVERIFY(lifecycle.state() == LifecycleState::Disabled);
    const LifecycleTransition start = lifecycle.requestStart(0);
    QVERIFY(!start.accepted);
    QVERIFY(start.message.contains(QStringLiteral("crashLimit")));
    QVERIFY(!lifecycle.resetDisabled().accepted);
}

QTEST_GUILESS_MAIN(LifecycleTest)
#include "tst_lifecycle.moc"
