// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/session_lock_state/session_lock_state_monitor.h"

#include "support/fake_session_lock_transport.h"

#include <QMetaProperty>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::SessionLockState;
using namespace QindaQt::Services::SessionLockState::TestSupport;

class SessionLockAuthenticationTests final : public QObject {
    Q_OBJECT

private slots:
    void startsFailClosedAndRequiresValidLocalSetup();
    void authenticatesAnExactThreeNameOwnerAndPidQuorum();
    void rejectsMalformedMismatchedAndWrongPidAuthorities();
    void rejectsSignalSubscriptionFailure();
    void ownerEventsInvalidateEveryPendingGeneration();
};

void SessionLockAuthenticationTests::startsFailClosedAndRequiresValidLocalSetup()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());
    const int propertyIndex = monitor.metaObject()->indexOfProperty("contentMayBeShown");
    QVERIFY(propertyIndex >= 0);
    QVERIFY(!monitor.metaObject()->property(propertyIndex).isWritable());

    QString error;
    QVERIFY2(monitor.start(&error), qPrintable(error));
    QCOMPARE(transport.ownerRequests.size(), 3);
    QCOMPARE(transport.events.constFirst(), QStringLiteral("start"));
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(monitor.start(&error));
    QCOMPARE(transport.startCount, 1);
    monitor.stop();
    QVERIFY(!monitor.isStarted());
    QCOMPARE(monitor.state(), LockState::Unknown);

    FakeSessionLockTransport rejected;
    rejected.startAccepted = false;
    SessionLockStateMonitor rejectedMonitor(rejected, 4242);
    QVERIFY(!rejectedMonitor.start(&error));
    QCOMPARE(error, QStringLiteral("injected start failure"));
    QCOMPARE(rejectedMonitor.state(), LockState::Unknown);

    FakeSessionLockTransport invalidPidTransport;
    SessionLockStateMonitor invalidPid(invalidPidTransport, 0);
    QVERIFY(!invalidPid.start(&error));
    QCOMPARE(invalidPidTransport.startCount, 0);

    FakeSessionLockTransport invalidPolicyTransport;
    SessionLockStateMonitor invalidPolicy(
        invalidPolicyTransport, 4242, SessionLockRetryPolicy{{0}});
    QVERIFY(!invalidPolicy.start(&error));
    QCOMPARE(invalidPolicyTransport.startCount, 0);
}

void SessionLockAuthenticationTests::authenticatesAnExactThreeNameOwnerAndPidQuorum()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QVERIFY(monitor.start());

    resolveMatchingOwners(transport);
    QCOMPARE(transport.processIdRequests.size(), 1);
    QCOMPARE(transport.processIdRequests.constFirst().owner,
             QStringLiteral(":1.42"));
    transport.resolvePid(transport.processIdRequests.constFirst(), 4242);
    QCOMPARE(transport.subscribedOwner, QStringLiteral(":1.42"));
    QCOMPARE(transport.activeRequests.size(), 1);
    QVERIFY(transport.events.indexOf(QStringLiteral("subscribe")) <
            transport.events.indexOf(QStringLiteral("active")));

    // Double false confirmation is required before content may be shown.
    transport.resolveActive(transport.activeRequests.constLast(), false);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QCOMPARE(transport.activeRequests.size(), 2);
    transport.resolveActive(transport.activeRequests.constLast(), false);
    QCOMPARE(monitor.state(), LockState::Unlocked);
    QVERIFY(monitor.contentMayBeShown());
}

void SessionLockAuthenticationTests::rejectsMalformedMismatchedAndWrongPidAuthorities()
{
    auto runOwners = [](const QStringList &owners) {
        FakeSessionLockTransport transport;
        SessionLockStateMonitor monitor(transport, 4242);
        if (!monitor.start()) {
            return qsizetype{-1};
        }
        const auto requests = transport.ownerRequests;
        if (requests.size() != 3 || owners.size() != 3) {
            return qsizetype{-1};
        }
        for (qsizetype index = 0; index < requests.size(); ++index) {
            transport.resolveOwner(requests.at(index), owners.at(index));
        }
        return transport.processIdRequests.size();
    };

    QCOMPARE(runOwners({QStringLiteral(":1.8"), QStringLiteral(":1.8"),
                        QStringLiteral(":1.9")}), 0);
    QCOMPARE(runOwners({QStringLiteral("org.not.unique"),
                        QStringLiteral(":1.8"), QStringLiteral(":1.8")}), 0);
    QCOMPARE(runOwners({QString{}, QStringLiteral(":1.8"),
                        QStringLiteral(":1.8")}), 0);

    FakeSessionLockTransport wrongPid;
    SessionLockStateMonitor monitor(wrongPid, 4242);
    QVERIFY(monitor.start());
    resolveMatchingOwners(wrongPid);
    wrongPid.resolvePid(wrongPid.processIdRequests.constFirst(), 4243);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(wrongPid.subscribedOwner.isEmpty());
    QVERIFY(wrongPid.activeRequests.isEmpty());
}

void SessionLockAuthenticationTests::rejectsSignalSubscriptionFailure()
{
    FakeSessionLockTransport transport;
    transport.subscriptionAccepted = false;
    SessionLockStateMonitor monitor(transport, 4242);
    QVERIFY(monitor.start());
    resolveMatchingOwners(transport);
    transport.resolvePid(transport.processIdRequests.constFirst(), 4242);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());
    QVERIFY(transport.activeRequests.isEmpty());
}

void SessionLockAuthenticationTests::ownerEventsInvalidateEveryPendingGeneration()
{
    FakeSessionLockTransport transport;
    SessionLockStateMonitor monitor(transport, 4242);
    QVERIFY(monitor.start());
    const auto staleOwners = transport.ownerRequests;

    Q_EMIT transport.serviceOwnerChanged(ObservedService::KdeScreenSaver,
                                         QString{});
    QCOMPARE(monitor.state(), LockState::Unknown);
    QCOMPARE(transport.ownerRequests.size(), 6);
    const auto currentOwners = transport.ownerRequests.mid(3);

    for (const auto &stale : staleOwners) {
        transport.resolveOwner(stale, QStringLiteral(":1.42"));
    }
    QVERIFY(transport.processIdRequests.isEmpty());
    for (const auto &current : currentOwners) {
        transport.resolveOwner(current, QStringLiteral(":1.43"));
    }
    QCOMPARE(transport.processIdRequests.size(), 1);
    const auto stalePid = transport.processIdRequests.constFirst();

    Q_EMIT transport.serviceOwnerChanged(ObservedService::Compositor,
                                         QStringLiteral(":1.44"));
    transport.resolvePid(stalePid, 4242);
    QVERIFY(transport.activeRequests.isEmpty());
    QCOMPARE(monitor.state(), LockState::Unknown);
}

QTEST_GUILESS_MAIN(SessionLockAuthenticationTests)

#include "tst_session_lock_authentication.moc"
