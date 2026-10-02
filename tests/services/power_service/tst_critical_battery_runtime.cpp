// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/critical_runtime_fixture.h"
#include <QtTest>
using namespace QindaQt::Tests;
using namespace QindaQt::Power;
namespace {
bool setPreference(CriticalRuntime &row, const QString &key, const QVariant &value) {
    QString error;
    return row.settings->setUserValue(key, value, &error);
}
}
#define CONFIGURE(row, configuredAction) \
    QTRY_VERIFY(row.settings->canSetUserValue(QStringLiteral("power.critical.countdownSeconds"))); \
    QVERIFY(setPreference(row, QStringLiteral("power.critical.countdownSeconds"), 5)); \
    QTRY_COMPARE(row.settings->snapshot()->values.value(QStringLiteral("power.critical.countdownSeconds")).toInt(), 5); \
    QTRY_VERIFY(row.settings->canSetUserValue(QStringLiteral("power.critical.action"))); \
    QVERIFY(setPreference(row, QStringLiteral("power.critical.action"), configuredAction)); \
    QTRY_COMPARE(row.settings->snapshot()->values.value(QStringLiteral("power.critical.action")).toString(), configuredAction); \
    QTRY_VERIFY(row.settings->canSetUserValue(QStringLiteral("power.critical.action"))); \
    QTest::qWait(100)
class CriticalBatteryRuntimeTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void configuredActions_data();
    void configuredActions();
    void cancelSuppressesSameEpisode();
    void noneAndNonActionWarnings();
    void dormantAndLegacyHandoff();
    void fences_data();
    void fences();
    void staleCapabilityCannotDispatchAfterPreferencesChange();
    void regressedSettingsCannotCompleteCountdown();
    void uncertainActionNeverReplaysAfterRecovery();
    void lateNotificationReplyOnlyCleansOwnedId();
    void lostNotificationReplyNeverStartsCountdown();
    void boundedPreferences_data();
    void boundedPreferences();
};
void CriticalBatteryRuntimeTests::configuredActions_data() {
    QTest::addColumn<QString>("choice"); QTest::addColumn<QString>("method");
    QTest::newRow("suspend") << QStringLiteral("suspend") << QStringLiteral("Suspend");
    QTest::newRow("hibernate") << QStringLiteral("hibernate") << QStringLiteral("Hibernate");
    QTest::newRow("power-off") << QStringLiteral("power-off") << QStringLiteral("PowerOff");
}
void CriticalBatteryRuntimeTests::configuredActions() {
    QFETCH(QString, choice); QFETCH(QString, method);
    CriticalRuntime row; QVERIFY(row.start()); CONFIGURE(row, choice);
    row.source(true, 5); QTRY_COMPARE(row.notifications(), 1);
    const auto initial = row.notification();
    QVERIFY(initial.expiresAtMs.has_value());
    QVERIFY(initial.body.contains(QStringLiteral("5 seconds")));
    QCOMPARE(initial.actions.size(), 1);
    // Authenticated external producers cannot cancel another producer's timer.
    auto spoof = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("ActionInvoked"));
    spoof.setArguments({initial.id, initial.actions.first().key});
    QVERIFY(row.clientBus.send(spoof));
    for (int i = 0; i < 3; ++i) { row.source(true, 5); QTest::qWait(150); }
    QTRY_VERIFY(row.notification().body != initial.body);
    QCOMPARE(row.notification().id, initial.id);
    QTRY_COMPARE_WITH_TIMEOUT(row.action->actions.size(), 1, 6500);
    QCOMPARE(row.action->actions.first(), method);
    QCOMPARE(row.notifications(), 0);
    QCOMPARE(row.action->directSleepCalls, 0);
    if (choice == QStringLiteral("power-off")) QVERIFY(row.action->nonInteractive);
    row.source(true, 5); QTest::qWait(150);
    QCOMPARE(row.action->actions.size(), 1);
    QCOMPARE(row.notifications(), 0);
    QCOMPARE(row.power->snapshot().supportedIdleInhibitorScopes.toInt(), 0);
}
void CriticalBatteryRuntimeTests::cancelSuppressesSameEpisode() {
    CriticalRuntime row; QVERIFY(row.start()); CONFIGURE(row, QStringLiteral("suspend"));
    row.source(true, 5); QTRY_COMPARE(row.notifications(), 1);
    const auto cancelledId = row.notification().id;
    QVERIFY(row.cancelFromPresenter());
    QTRY_COMPARE(row.notifications(), 0);
    row.source(true, 5); QTest::qWait(5400);
    QVERIFY(row.action->actions.isEmpty());
    QCOMPARE(row.notifications(), 0);
    row.source(false); QTRY_VERIFY(!row.power->snapshot().source.onBattery);
    row.source(true, 5); QTRY_COMPARE(row.notifications(), 1);
    // Recovered critical episode has a distinct owned notification identity.
    QVERIFY(row.notification().id != cancelledId);
    QVERIFY(row.cancelFromPresenter()); QTRY_COMPARE(row.notifications(), 0);
    QVERIFY(row.action->actions.isEmpty());
}
void CriticalBatteryRuntimeTests::noneAndNonActionWarnings() {
    CriticalRuntime row; QVERIFY(row.start()); CONFIGURE(row, QStringLiteral("none"));
    row.source(true, 5); QTest::qWait(200); QCOMPARE(row.notifications(), 0);
    row.source(true, 4); CONFIGURE(row, QStringLiteral("hibernate"));
    QTest::qWait(200); QCOMPARE(row.notifications(), 0);
    row.source(false, 5); QTest::qWait(200); QCOMPARE(row.notifications(), 0);
    row.source(true, 5); QTRY_COMPARE(row.notifications(), 1);
    row.source(false); QTRY_COMPARE(row.notifications(), 0);
    QVERIFY(row.action->actions.isEmpty());
}
void CriticalBatteryRuntimeTests::dormantAndLegacyHandoff() {
    {
        CriticalRuntime row; QVERIFY(row.start(false)); CONFIGURE(row, QStringLiteral("suspend"));
        row.source(true, 5); QTest::qWait(200); QCOMPARE(row.notifications(), 0);
        QVERIFY(row.action->actions.isEmpty());
    }
    CriticalRuntime row; QVERIFY(row.start(true, true)); CONFIGURE(row, QStringLiteral("suspend"));
    row.source(true, 5); QTest::qWait(200); QCOMPARE(row.notifications(), 0);
    QVERIFY(row.legacyBus.unregisterService(QStringLiteral("org.kde.Solid.PowerManagement")));
    QTRY_COMPARE(row.notifications(), 1);
    QVERIFY(row.legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement")));
    QTRY_COMPARE(row.notifications(), 0);
    QVERIFY(row.legacyBus.unregisterService(QStringLiteral("org.kde.Solid.PowerManagement")));
    QTest::qWait(5400); QCOMPARE(row.notifications(), 0);
    QVERIFY(row.action->actions.isEmpty());
}
void CriticalBatteryRuntimeTests::fences_data() {
    QTest::addColumn<QString>("boundary");
    for (const char *name : {"ac", "warning", "unknown", "settings", "upower", "notifications", "sleep"})
        QTest::newRow(name) << QString::fromLatin1(name);
}
void CriticalBatteryRuntimeTests::fences() {
    QFETCH(QString, boundary);
    CriticalRuntime row; QVERIFY(row.start()); CONFIGURE(row, QStringLiteral("suspend"));
    row.source(true, 5); QTRY_COMPARE(row.notifications(), 1);
    if (boundary == QStringLiteral("ac")) row.source(false);
    else if (boundary == QStringLiteral("warning")) row.source(true, 4);
    else if (boundary == QStringLiteral("unknown")) row.source(true, 0);
    else if (boundary == QStringLiteral("settings")) row.service.reset();
    else if (boundary == QStringLiteral("upower")) row.upower->unregisterService();
    else if (boundary == QStringLiteral("notifications")) row.host->stop();
    else row.action->stop();
    if (boundary != QStringLiteral("notifications")) QTRY_COMPARE(row.notifications(), 0);
    QTest::qWait(5400);
    QVERIFY(row.action->actions.isEmpty());
}
void CriticalBatteryRuntimeTests::staleCapabilityCannotDispatchAfterPreferencesChange() {
    CriticalRuntime row; QVERIFY(row.start()); CONFIGURE(row, QStringLiteral("hibernate"));
    row.source(true, 5); QTRY_COMPARE(row.notifications(), 1);
    row.action->holdCan = true;
    QTRY_VERIFY_WITH_TIMEOUT(row.action->delayedCan.has_value(), 6500);
    QTRY_COMPARE(row.notifications(), 0);
    QTRY_VERIFY(row.settings->canSetUserValue(QStringLiteral("power.critical.action")));
    QVERIFY(setPreference(row, QStringLiteral("power.critical.action"), QStringLiteral("none")));
    QTRY_COMPARE(row.settings->snapshot()->values.value(QStringLiteral("power.critical.action")).toString(), QStringLiteral("none"));
    QTest::qWait(100); row.action->replyCapability(); QTest::qWait(200);
    QVERIFY(row.action->actions.isEmpty());
}
void CriticalBatteryRuntimeTests::regressedSettingsCannotCompleteCountdown() {
    CriticalRuntime row; QVERIFY(row.start(true, false, true));
    row.source(true, 5); QTRY_COMPARE(row.notifications(), 1);
    row.adversarial->revision = 9;
    row.adversarial->values[QStringLiteral("power.critical.action")] = QStringLiteral("hibernate");
    row.adversarial->invalidate(11);
    QTRY_COMPARE(row.notifications(), 0); QTest::qWait(5400);
    QVERIFY(row.action->actions.isEmpty());
}
void CriticalBatteryRuntimeTests::uncertainActionNeverReplaysAfterRecovery() {
    CriticalRuntime row; QVERIFY(row.start()); CONFIGURE(row, QStringLiteral("suspend"));
    row.action->uncertain = true;
    row.source(true, 5); QTRY_COMPARE_WITH_TIMEOUT(row.action->actions.size(), 1, 6500);
    QTRY_COMPARE(row.notifications(), 0); QTest::qWait(150);
    row.source(false); QTRY_VERIFY(!row.power->snapshot().source.onBattery);
    row.source(true, 5); QTest::qWait(250);
    QCOMPARE(row.action->actions.size(), 1); QCOMPARE(row.notifications(), 0);
}
void CriticalBatteryRuntimeTests::lateNotificationReplyOnlyCleansOwnedId() {
    CriticalRuntime row; QVERIFY(row.start(true, false, false, true)); CONFIGURE(row, QStringLiteral("suspend"));
    row.source(true, 5); QTRY_VERIFY(row.fault->pending.has_value());
    QVERIFY(setPreference(row, QStringLiteral("power.critical.action"), QStringLiteral("none")));
    QTRY_COMPARE(row.settings->snapshot()->values.value(QStringLiteral("power.critical.action")).toString(), QStringLiteral("none"));
    QTest::qWait(100); row.fault->replyNotify();
    QTRY_COMPARE(row.fault->closedIds, QList<quint32>{41});
    QVERIFY(!row.fault->retained); QCOMPARE(row.fault->notifyCount, 1);
    QVERIFY(row.action->actions.isEmpty());
}
void CriticalBatteryRuntimeTests::lostNotificationReplyNeverStartsCountdown() {
    CriticalRuntime row; QVERIFY(row.start(true, false, false, true)); CONFIGURE(row, QStringLiteral("suspend"));
    row.source(true, 5); QTRY_COMPARE(row.fault->notifyCount, 1);
    QCOMPARE(row.fault->expiryMilliseconds, 8000);
    QTest::qWait(5400); row.source(true, 5); QTest::qWait(150);
    QCOMPARE(row.fault->notifyCount, 1); QVERIFY(row.fault->closedIds.isEmpty());
    QVERIFY(row.action->actions.isEmpty());
    // There was no confirmed ID to close; the explicitly bounded server
    // expiry retires the otherwise unaddressable retained notification.
    QTRY_VERIFY_WITH_TIMEOUT(!row.fault->retained, 3000);
}
void CriticalBatteryRuntimeTests::boundedPreferences_data() {
    QTest::addColumn<QVariant>("seconds"); QTest::addColumn<bool>("admitted");
    QTest::newRow("too-short") << QVariant(4) << false;
    QTest::newRow("too-long") << QVariant(301) << false;
    QTest::newRow("string-is-not-integer") << QVariant(QStringLiteral("5")) << false;
    QTest::newRow("bool-is-not-integer") << QVariant(true) << false;
    QTest::newRow("maximum") << QVariant(300) << true;
}
void CriticalBatteryRuntimeTests::boundedPreferences() {
    QFETCH(QVariant, seconds); QFETCH(bool, admitted);
    CriticalRuntime row; QVERIFY(row.start(true, false, true));
    QTRY_VERIFY(row.settings->snapshot().has_value());
    row.adversarial->revision = 11;
    row.adversarial->values[QStringLiteral("power.critical.countdownSeconds")] = seconds;
    row.adversarial->invalidate(11); QTest::qWait(150);
    row.source(true, 5);
    if (admitted) {
        QTRY_COMPARE(row.notifications(), 1);
        QVERIFY(row.notification().body.contains(QStringLiteral("300 seconds")));
        QVERIFY(row.cancelFromPresenter()); QTRY_COMPARE(row.notifications(), 0);
    } else { QTest::qWait(200); QCOMPARE(row.notifications(), 0); }
    QVERIFY(row.action->actions.isEmpty());
}
QTEST_GUILESS_MAIN(CriticalBatteryRuntimeTests)
#include "tst_critical_battery_runtime.moc"
