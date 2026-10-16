// SPDX-License-Identifier: GPL-3.0-or-later

#include "session_actions_fakes.h"

using namespace QindaQt::Services::SessionActions;
using namespace SessionActionsFakes;


class SessionActionsClientTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void canChecksPublishTypedFailClosedTruth();
    void ownedScreenSaverWithoutInterfaceIsUnavailable();
    void dispatchRepeatsAdmissionAndSerializes();
    void delayedSuccessFromReplacedOwnerIsUncertain();
    void mutationTimeoutIsUncertainAndNeverReplays();
    void ownerLossWithdrawsAvailabilityWithoutPolling();
    void logindAloneCannotOfferSuspend();
    void nativeSleepInvalidationRefreshesAvailability();
    void hibernateRepeatsProtectedCurrentOwnerAdmission();
};

void SessionActionsClientTest::canChecksPublishTypedFailClosedTruth()
{
    PrivateServices services;
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start();
    QTRY_VERIFY(client.canLock());
    QTRY_VERIFY(client.canLogout());
    QTRY_VERIFY(client.canSuspend());
    QVERIFY(!client.canReboot());
    QVERIFY(!client.canPowerOff());

    const SessionActionAvailability expected{
        .lock = true, .logout = true, .suspend = true,
        .reboot = false, .powerOff = false};
    QCOMPARE(client.availability(), expected);
    QVERIFY(services.session.canCount >= 1);
    QVERIFY(services.sleep.canCount >= 1);
    QVERIFY(!client.requestReboot());
    QCOMPARE(services.logind.rebootCount, 0);
    QVERIFY(client.feedback().contains(QStringLiteral("unavailable")));
}

void SessionActionsClientTest::ownedScreenSaverWithoutInterfaceIsUnavailable()
{
    PrivateServices services(false);
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start();
    QTRY_VERIFY(client.canLogout());

    QVERIFY(!client.canLock());
    QVERIFY(!client.requestLock());
    QCOMPARE(services.screenSaver.lockCount, 0);
    QVERIFY(client.feedback().contains(QStringLiteral("unavailable")));
}

void SessionActionsClientTest::dispatchRepeatsAdmissionAndSerializes()
{
    PrivateServices services;
    SessionActionsClient client(services.clientBus, services.clientBus);
    QSignalSpy finished(&client, &SessionActionsClient::actionFinished);
    client.start();
    QTRY_VERIFY(client.canSuspend());
    const int baselineCan = services.sleep.canCount;

    QVERIFY(client.requestSuspend());
    QVERIFY(client.pending());
    QVERIFY(!client.requestLock());
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(services.sleep.canCount >= baselineCan + 1);
    QCOMPARE(services.sleep.suspendCount, 1);
    QCOMPARE(services.logind.suspendCount, 0);
    QVERIFY(!client.pending());
    const auto result = qvariant_cast<SessionActionResult>(finished.first().first());
    QCOMPARE(result.action, SessionAction::Suspend);
    QCOMPARE(result.status, ActionStatus::Succeeded);

    const int baselineGetActive = services.screenSaver.getActiveCount;
    QVERIFY(client.requestLock());
    QTRY_COMPARE(services.screenSaver.lockCount, 1);
    QVERIFY(services.screenSaver.getActiveCount >= baselineGetActive + 1);
    QTRY_COMPARE(finished.size(), 2);

    const int baselineLogoutCan = services.session.canCount;
    QVERIFY(client.requestLogout());
    QTRY_COMPARE(services.session.logoutCount, 1);
    QTRY_COMPARE(finished.size(), 3);
    QVERIFY(services.session.canCount >= baselineLogoutCan + 1);
}

void SessionActionsClientTest::delayedSuccessFromReplacedOwnerIsUncertain()
{
    PrivateServices services;
    services.screenSaver.holdReply = true;
    SessionActionsClient client(services.clientBus, services.clientBus);
    QSignalSpy finished(&client, &SessionActionsClient::actionFinished);
    client.start();
    QTRY_VERIFY(client.canLock());

    QVERIFY(client.requestLock());
    QTRY_COMPARE(services.screenSaver.lockCount, 1);
    QTRY_VERIFY(services.screenSaver.delayedLockReply.has_value());

    const QString replacementName =
        QStringLiteral("qindaqt-session-actions-replacement-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QDBusConnection replacementBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, replacementName);
    FakeScreenSaver replacement;
    QVERIFY(replacementBus.isConnected());
    QVERIFY(services.bus.unregisterService(
        QStringLiteral("org.freedesktop.ScreenSaver")));
    QVERIFY(replacementBus.registerService(
        QStringLiteral("org.freedesktop.ScreenSaver")));
    QVERIFY(replacementBus.registerObject(QStringLiteral("/ScreenSaver"),
                                          &replacement,
                                          QDBusConnection::ExportAllSlots));
    const auto replacementCleanup = qScopeGuard([&] {
        replacementBus.unregisterObject(QStringLiteral("/ScreenSaver"));
        replacementBus.unregisterService(QStringLiteral("org.freedesktop.ScreenSaver"));
        QDBusConnection::disconnectFromBus(replacementName);
    });
    QVERIFY(services.screenSaver.sendDelayedLockReply(services.bus));

    QTRY_COMPARE(finished.size(), 1);
    const auto result = qvariant_cast<SessionActionResult>(finished.first().first());
    QCOMPARE(result.action, SessionAction::Lock);
    QCOMPARE(result.status, ActionStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("authority-replaced"));
    QCOMPARE(services.screenSaver.lockCount, 1);
    QCOMPARE(replacement.lockCount, 0);
}

void SessionActionsClientTest::ownerLossWithdrawsAvailabilityWithoutPolling()
{
    PrivateServices services;
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start();
    QTRY_VERIFY(client.canSuspend());
    const int baselineCan = services.sleep.canCount;
    QTest::qWait(50);
    QCOMPARE(services.sleep.canCount, baselineCan);

    services.bus.unregisterObject(QStringLiteral("/org/qindaqt/Sleep1"));
    services.bus.unregisterService(QStringLiteral("org.qindaqt.Sleep1"));
    QTRY_VERIFY(!client.canSuspend());
    QVERIFY(!client.canReboot());
    QVERIFY(!client.canPowerOff());
    QVERIFY(!client.requestSuspend());
    QCOMPARE(services.logind.suspendCount, 0);
}

void SessionActionsClientTest::mutationTimeoutIsUncertainAndNeverReplays()
{
    PrivateServices services;
    services.screenSaver.holdReply = true;
    SessionActionsClient client(services.clientBus, services.clientBus);
    QSignalSpy finished(&client, &SessionActionsClient::actionFinished);
    client.start();
    QTRY_VERIFY(client.canLock());

    QVERIFY(client.requestLock());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1,
                              SessionActionsClient::ActionTimeoutMilliseconds + 1'000);
    QCOMPARE(services.screenSaver.lockCount, 1);
    const auto result = qvariant_cast<SessionActionResult>(finished.first().first());
    QCOMPARE(result.action, SessionAction::Lock);
    QCOMPARE(result.status, ActionStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("request-timeout"));
    QTest::qWait(50);
    QCOMPARE(services.screenSaver.lockCount, 1);
}

void SessionActionsClientTest::logindAloneCannotOfferSuspend() {
    PrivateServices services;
    services.bus.unregisterService(QStringLiteral("org.qindaqt.Sleep1"));
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start(); QTRY_VERIFY(client.canLock());
    QTest::qWait(100); QVERIFY(!client.canSuspend()); QVERIFY(!client.requestSuspend());
    QCOMPARE(services.logind.suspendCount, 0);
}

void SessionActionsClientTest::nativeSleepInvalidationRefreshesAvailability() {
    PrivateServices services; services.sleep.allowed = false;
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start(); QTRY_VERIFY(client.canLock()); QVERIFY(!client.canSuspend());
    services.sleep.allowed = true; Q_EMIT services.sleep.Changed();
    QTRY_VERIFY(client.canSuspend());
    services.sleep.allowed = false; Q_EMIT services.sleep.Changed();
    QTRY_VERIFY(!client.canSuspend());
}

void SessionActionsClientTest::hibernateRepeatsProtectedCurrentOwnerAdmission()
{
    // The append-only enum preserves every existing action's public value.
    static_assert(int(SessionAction::PowerOff) == 4);
    static_assert(int(SessionAction::Hibernate) == 5);
    PrivateServices services;
    services.sleep.hibernateAllowed = true;
    SessionActionsClient client(services.clientBus, services.clientBus);
    QSignalSpy finished(&client, &SessionActionsClient::actionFinished);
    client.start();
    QTRY_VERIFY(client.canHibernate());
    QVERIFY(client.requestHibernate());
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(services.sleep.hibernateCount, 1);
    QCOMPARE(qvariant_cast<SessionActionResult>(finished.first().first()).action, SessionAction::Hibernate);
    QCOMPARE(qvariant_cast<SessionActionResult>(finished.first().first()).status, ActionStatus::Succeeded);
    // A previously admitted hint never skips the fresh CanHibernate query.
    services.sleep.hibernateAllowed = false;
    QVERIFY(client.requestHibernate());
    QTRY_COMPARE(finished.size(), 2);
    QCOMPARE(services.sleep.hibernateCount, 1);
    QCOMPARE(qvariant_cast<SessionActionResult>(finished.last().first()).status, ActionStatus::Rejected);
    services.bus.unregisterService(QStringLiteral("org.qindaqt.Session1"));
    QTRY_VERIFY(!client.canHibernate());
    QVERIFY(!client.requestHibernate());
    QCOMPARE(services.sleep.hibernateCount, 1);
}

QTEST_GUILESS_MAIN(SessionActionsClientTest)
#include "tst_session_actions_client.moc"
