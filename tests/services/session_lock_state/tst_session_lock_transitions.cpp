// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/session_lock_state/session_lock_state_monitor.h"

#include "support/fake_session_lock_transport.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::SessionLockState;
using namespace QindaQt::Services::SessionLockState::TestSupport;

class SessionLockTransitionTests final : public QObject {
    Q_OBJECT

private slots:
    void activeInitialReplyLocksImmediately();
    void fencesFalseRepliesAcrossLockAcquisition();
    void acceptsOnlyAuthenticatedOwnerSignals();
    void retriesOnlyServiceBeforeObjectFailuresWithinTheBound();
    void staleRetryIsFencedByOwnerChange();
    void transportLossRevokesUnlockedAuthority();
    void failuresAndStopAlwaysFailClosed();
};

void SessionLockTransitionTests::activeInitialReplyLocksImmediately()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QVERIFY(monitor.start());
    const auto query = authenticate(transport, 4242);
    transport.resolveActive(query, true);
    QCOMPARE(monitor.state(), LockState::Locked);
    QCOMPARE(transport.activeRequests.size(), 1);
    QVERIFY(!monitor.contentMayBeShown());
}

void SessionLockTransitionTests::fencesFalseRepliesAcrossLockAcquisition()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QSignalSpy mayShowSpy(&monitor,
                          &SessionLockStateMonitor::contentMayBeShownChanged);
    QVERIFY(monitor.start());
    const auto firstQuery = authenticate(transport, 4242);

    transport.resolveActive(firstQuery, false);
    QCOMPARE(monitor.state(), LockState::Unknown);
    const auto confirmation = transport.activeRequests.constLast();
    Q_EMIT transport.aboutToLock(QStringLiteral(":1.42"));
    QCOMPARE(monitor.state(), LockState::Locking);
    transport.resolveActive(confirmation, false);
    QCOMPARE(monitor.state(), LockState::Locking);
    QVERIFY(!monitor.contentMayBeShown());

    Q_EMIT transport.activeChanged(QStringLiteral(":1.42"), true);
    QCOMPARE(monitor.state(), LockState::Locked);
    Q_EMIT transport.activeChanged(QStringLiteral(":1.42"), false);
    QCOMPARE(monitor.state(), LockState::Unlocked);
    QCOMPARE(mayShowSpy.size(), 1);
    QCOMPARE(mayShowSpy.constFirst().constFirst().toBool(), true);
}

void SessionLockTransitionTests::acceptsOnlyAuthenticatedOwnerSignals()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QVERIFY(monitor.start());
    authenticate(transport, 4242);

    Q_EMIT transport.activeChanged(QStringLiteral(":1.99"), false);
    Q_EMIT transport.aboutToLock(QStringLiteral(":1.99"));
    QCOMPARE(monitor.state(), LockState::Unknown);
    Q_EMIT transport.activeChanged(QStringLiteral(":1.42"), true);
    QCOMPARE(monitor.state(), LockState::Locked);
}

void SessionLockTransitionTests::retriesOnlyServiceBeforeObjectFailuresWithinTheBound()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(
        transport, 4242, SessionLockRetryPolicy{{3, 7}});
    QVERIFY(monitor.start());
    auto query = authenticate(transport, 4242);

    transport.fail(query,
                   QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"));
    QCOMPARE(transport.retries.size(), 1);
    QCOMPARE(transport.retries.constLast().delayMilliseconds, 3);
    transport.fireRetry(transport.retries.constLast());
    query = transport.activeRequests.constLast();
    transport.fail(query,
                   QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"));
    QCOMPARE(transport.retries.size(), 2);
    QCOMPARE(transport.retries.constLast().delayMilliseconds, 7);
    transport.fireRetry(transport.retries.constLast());
    query = transport.activeRequests.constLast();
    transport.fail(query,
                   QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"));
    QCOMPARE(transport.retries.size(), 2);
    QCOMPARE(monitor.state(), LockState::Unknown);

    FakeSessionLockTransport timeoutTransport;
    SessionLockStateMonitor timeoutMonitor(
        timeoutTransport, 4242, SessionLockRetryPolicy{{3, 7}});
    QVERIFY(timeoutMonitor.start());
    const auto timeoutQuery = authenticate(timeoutTransport, 4242);
    timeoutTransport.fail(
        timeoutQuery, QStringLiteral("org.freedesktop.DBus.Error.Timeout"));
    QVERIFY(timeoutTransport.retries.isEmpty());
    QCOMPARE(timeoutMonitor.state(), LockState::Unknown);
}

void SessionLockTransitionTests::staleRetryIsFencedByOwnerChange()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(
        transport, 4242, SessionLockRetryPolicy{{3}});
    QVERIFY(monitor.start());
    const auto query = authenticate(transport, 4242);
    transport.fail(query,
                   QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"));
    QCOMPARE(transport.retries.size(), 1);
    const auto staleRetry = transport.retries.constFirst();

    Q_EMIT transport.serviceOwnerChanged(ObservedService::KdeScreenSaver,
                                         QStringLiteral(":1.43"));
    const qsizetype activeCount = transport.activeRequests.size();
    transport.fireRetry(staleRetry);
    QCOMPARE(transport.activeRequests.size(), activeCount);
    QCOMPARE(monitor.state(), LockState::Unknown);
}

void SessionLockTransitionTests::transportLossRevokesUnlockedAuthority()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QSignalSpy mayShowSpy(&monitor,
                          &SessionLockStateMonitor::contentMayBeShownChanged);
    QVERIFY(monitor.start());
    auto query = authenticate(transport, 4242);
    transport.resolveActive(query, false);
    query = transport.activeRequests.constLast();
    transport.resolveActive(query, false);
    QCOMPARE(monitor.state(), LockState::Unlocked);

    const auto staleQuery = query;
    transport.loseTransport();
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());
    QVERIFY(!monitor.isStarted());
    QCOMPARE(mayShowSpy.size(), 2);
    QCOMPARE(mayShowSpy.constLast().constFirst().toBool(), false);

    transport.resolveActive(staleQuery, false);
    Q_EMIT transport.activeChanged(QStringLiteral(":1.42"), false);
    QCOMPARE(monitor.state(), LockState::Unknown);
}

void SessionLockTransitionTests::failuresAndStopAlwaysFailClosed()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QSignalSpy mayShowSpy(&monitor,
                          &SessionLockStateMonitor::contentMayBeShownChanged);
    QVERIFY(monitor.start());
    auto query = authenticate(transport, 4242);
    transport.resolveActive(query, false);
    query = transport.activeRequests.constLast();
    transport.resolveActive(query, false);
    QCOMPARE(monitor.state(), LockState::Unlocked);

    Q_EMIT transport.serviceOwnerChanged(ObservedService::Compositor,
                                         QString{});
    QCOMPARE(monitor.state(), LockState::Unknown);
    QCOMPARE(mayShowSpy.size(), 2);
    QCOMPARE(mayShowSpy.constLast().constFirst().toBool(), false);

    // Re-authenticate so stop itself proves an Unlocked -> Unknown transition.
    const auto ownerRequests = transport.ownerRequests.mid(3);
    for (const auto &ownerRequest : ownerRequests) {
        transport.resolveOwner(ownerRequest, QStringLiteral(":1.43"));
    }
    transport.resolvePid(transport.processIdRequests.constLast(), 4242);
    query = transport.activeRequests.constLast();
    transport.resolveActive(query, false);
    query = transport.activeRequests.constLast();
    transport.resolveActive(query, false);
    QCOMPARE(monitor.state(), LockState::Unlocked);

    monitor.stop();
    QCOMPARE(monitor.state(), LockState::Unknown);
    Q_EMIT transport.activeChanged(QStringLiteral(":1.42"), false);
    QCOMPARE(monitor.state(), LockState::Unknown);
}

QTEST_GUILESS_MAIN(SessionLockTransitionTests)

#include "tst_session_lock_transitions.moc"
