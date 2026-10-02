// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/lid_runtime_fixture.h"
#include <QtTest>
using namespace QindaQt::Tests;
using namespace QindaQt::Power;
#define PREPARE(row) QVERIFY(row.prepare()); QVERIFY(row.start()); \
    QTRY_VERIFY(row.power->snapshot().capabilities.testFlag(Capability::Lid))
#define CONFIGURE(row, source, kind, value) \
    const auto key = QStringLiteral("power.lid.") + source + QLatin1Char('.') + kind; \
    QTRY_VERIFY(row.settings->canSetUserValue(key)); QVERIFY(row.preference(key, value)); \
    QTRY_COMPARE(row.settings->snapshot()->values.value(key).toString(), value); \
    QTRY_VERIFY(row.settings->canSetUserValue(key)); QTest::qWait(150)
#define OWNED(row) do { QTRY_VERIFY(row.logind->hasLiveOwned()); QTest::qWait(150); } while (false)
class LidRuntimeTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void firstGenuineClose_data(); void firstGenuineClose();
    void sourceAndDockSelection_data(); void sourceAndDockSelection();
    void defaultOffAndProductionUid();
    void nonePreservesExternalInhibitors();
    void startupClosedPreferencesAndReadinessAreNotEdges();
    void legacyArrivalAndReturnDoNotReplay();
    void admissionFailures_data(); void admissionFailures();
    void loss_data(); void loss();
    void lateFdReplyOnlyClosesItsOwnDescriptor();
    void lateFdOwnerReplacementPreservesNewDescriptor();
    void fdTimeoutNeverReplays();
    void pendingCanFence_data(); void pendingCanFence();
    void dispatchedUncertaintyNeverReplays();
    void spoofedSessionInvalidationCannotRevoke();
    void shutdownClosesEveryOwnedDuplicate();
};
void LidRuntimeTests::firstGenuineClose_data() {
    QTest::addColumn<QString>("choice"); QTest::addColumn<QString>("method");
    QTest::newRow("suspend") << QStringLiteral("suspend") << QStringLiteral("Suspend");
    QTest::newRow("hibernate") << QStringLiteral("hibernate") << QStringLiteral("Hibernate");
    QTest::newRow("lock") << QStringLiteral("lock") << QStringLiteral("Lock");
    QTest::newRow("power-off") << QStringLiteral("power-off") << QStringLiteral("PowerOff");
}
void LidRuntimeTests::firstGenuineClose() {
    QFETCH(QString, choice); QFETCH(QString, method);
    LidRuntime row; PREPARE(row); CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), choice); OWNED(row);
    QVERIFY(!row.power->snapshot().source.lidClosed); QVERIFY(!row.power->snapshot().source.lidPresent);
    QCOMPARE(row.logind->queriedPid, quint32(::getpid()));
    const int acquisitions = row.logind->inhibitCalls;
    const auto args = row.logind->arguments.last(); QCOMPARE(args.first().toString(), QStringLiteral("handle-lid-switch")); QCOMPARE(args.last().toString(), QStringLiteral("block"));
    row.logind->lid(true); QTRY_COMPARE(row.actions(), QStringList{method});
    QVERIFY(row.power->snapshot().source.lidPresent);
    for (int i = 0; i < 4; ++i) { row.logind->lid(true); row.source(false); QTest::qWait(80); }
    QCOMPARE(row.actions(), QStringList{method}); QCOMPARE(row.logind->inhibitCalls, acquisitions);
    QCOMPARE(row.logind->directSleepCalls, 0);
    if (choice == QStringLiteral("power-off")) QVERIFY(!row.logind->domain.actionCalls.first().interactive);
    QCOMPARE(row.power->supportedIdleInhibitorScopes(), IdleInhibitorScopes{});
    row.logind->lid(false); QTRY_VERIFY(!row.power->snapshot().source.lidClosed); QTest::qWait(100);
    row.logind->lid(true); QTRY_COMPARE(row.actions().size(), 2);
}
void LidRuntimeTests::sourceAndDockSelection_data() {
    QTest::addColumn<QString>("source"); QTest::addColumn<bool>("docked");
    QTest::newRow("ac-docked") << QStringLiteral("ac") << true;
    QTest::newRow("battery") << QStringLiteral("battery") << false;
    QTest::newRow("low-battery") << QStringLiteral("lowBattery") << false;
}
void LidRuntimeTests::sourceAndDockSelection() {
    QFETCH(QString, source); QFETCH(bool, docked);
    LidRuntime row; PREPARE(row);
    CONFIGURE(row, source, docked ? QStringLiteral("dockedAction") : QStringLiteral("action"), QStringLiteral("hibernate"));
    row.source(source != QStringLiteral("ac"), source == QStringLiteral("lowBattery") ? 3u : 2u);
    row.logind->lid(false, docked); QTest::qWait(150); OWNED(row);
    row.logind->lid(true, docked); QTRY_COMPARE(row.actions(), QStringList{QStringLiteral("Hibernate")});
}
void LidRuntimeTests::defaultOffAndProductionUid() {
    for (const bool installed : {false, true}) {
        LidRuntime row; QVERIFY(row.prepare()); QVERIFY(row.start(installed, installed));
        CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend"));
        row.logind->lid(true); QTest::qWait(300);
        QVERIFY(row.actions().isEmpty()); QCOMPARE(row.logind->inhibitCalls, 0);
        QVERIFY(row.logind->externalAlive());
    }
}
void LidRuntimeTests::nonePreservesExternalInhibitors() {
    LidRuntime row; PREPARE(row); CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("none")); OWNED(row);
    row.logind->lid(true); QTest::qWait(200); QVERIFY(row.actions().isEmpty());
    QVERIFY(row.legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement")));
    QTRY_VERIFY(row.logind->allClosed()); QVERIFY(row.logind->externalAlive());
    QTRY_VERIFY(row.power->snapshot().inhibitors.size() == 1);
    QCOMPARE(row.power->snapshot().inhibitors.first().who, QStringLiteral("External app"));
}
void LidRuntimeTests::startupClosedPreferencesAndReadinessAreNotEdges() {
    LidRuntime row; QVERIFY(row.prepare(true)); QVERIFY(row.start());
    CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend")); OWNED(row);
    row.logind->activity(false); QTRY_VERIFY(row.logind->allClosed());
    row.logind->activity(true); OWNED(row); QTest::qWait(200); QVERIFY(row.actions().isEmpty());
    row.logind->lid(false); QTRY_VERIFY(!row.power->snapshot().source.lidClosed); QTest::qWait(100);
    row.logind->lid(true); QTRY_COMPARE(row.actions(), QStringList{QStringLiteral("Suspend")});
}
void LidRuntimeTests::legacyArrivalAndReturnDoNotReplay() {
    LidRuntime row; QVERIFY(row.prepare(true, true)); QVERIFY(row.start());
    CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend"));
    QCOMPARE(row.logind->inhibitCalls, 0);
    QVERIFY(row.legacyBus.unregisterService(QStringLiteral("org.kde.Solid.PowerManagement"))); OWNED(row);
    QVERIFY(row.actions().isEmpty());
    QVERIFY(row.legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement"))); QTRY_VERIFY(row.logind->allClosed());
    QVERIFY(row.legacyBus.unregisterService(QStringLiteral("org.kde.Solid.PowerManagement"))); OWNED(row);
    row.logind->lid(true); QTest::qWait(200); QVERIFY(row.actions().isEmpty());
    row.logind->lid(false); QTRY_VERIFY(!row.power->snapshot().source.lidClosed); QTest::qWait(100);
    row.logind->lid(true); QTRY_COMPARE(row.actions().size(), 1);
}
void LidRuntimeTests::admissionFailures_data() {
    QTest::addColumn<QString>("fault");
    for (const char *name : {"inactive", "wrong-user", "wrong-pid", "malformed-active", "denied-fd", "screen-off"}) QTest::newRow(name) << QString::fromLatin1(name);
}
void LidRuntimeTests::admissionFailures() {
    QFETCH(QString, fault);
    LidRuntime row; QVERIFY(row.prepare());
    if (fault == QStringLiteral("inactive")) row.logind->active = false;
    else if (fault == QStringLiteral("wrong-user")) row.logind->wrongUser = true;
    else if (fault == QStringLiteral("wrong-pid")) row.logind->pidAccepted = false;
    else if (fault == QStringLiteral("malformed-active")) row.logind->malformedActive = true;
    else if (fault == QStringLiteral("denied-fd")) row.logind->denyInhibit = true;
    QVERIFY(row.start());
    CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), fault == QStringLiteral("screen-off") ? QStringLiteral("screen-off") : QStringLiteral("suspend"));
    row.logind->lid(true); QTest::qWait(350); QVERIFY(row.actions().isEmpty());
    QVERIFY(!row.logind->hasLiveOwned());
}
void LidRuntimeTests::loss_data() {
    QTest::addColumn<QString>("boundary");
    for (const char *name : {"login1", "session-owner", "session-removed", "inactive", "settings", "upower"}) QTest::newRow(name) << QString::fromLatin1(name);
}
void LidRuntimeTests::loss() {
    QFETCH(QString, boundary);
    LidRuntime row; PREPARE(row); CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend")); OWNED(row);
    if (boundary == QStringLiteral("login1")) row.logind->stop();
    else if (boundary == QStringLiteral("session-owner")) QVERIFY(row.sessionBus.unregisterService(QStringLiteral("org.qindaqt.Session1")));
    else if (boundary == QStringLiteral("session-removed")) row.logind->removed();
    else if (boundary == QStringLiteral("inactive")) row.logind->activity(false);
    else if (boundary == QStringLiteral("settings")) row.service.reset();
    else row.upower->unregisterService();
    QTRY_VERIFY(row.logind->allClosed()); row.logind->lid(true); QTest::qWait(200);
    QVERIFY(row.actions().isEmpty()); QVERIFY(row.logind->externalAlive());
}
void LidRuntimeTests::lateFdReplyOnlyClosesItsOwnDescriptor() {
    LidRuntime row; QVERIFY(row.prepare()); row.logind->holdInhibit = true; QVERIFY(row.start());
    CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend"));
    QTRY_VERIFY(row.logind->delayedInhibit.has_value());
    QVERIFY(row.legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement"))); QTest::qWait(100);
    row.logind->replyInhibit(); QTRY_VERIFY(!row.logind->peers.isEmpty()); QTRY_VERIFY(row.logind->allClosed());
    row.logind->lid(true); QTest::qWait(200); QVERIFY(row.actions().isEmpty()); QVERIFY(row.logind->externalAlive());
}
void LidRuntimeTests::fdTimeoutNeverReplays() {
    LidRuntime row; QVERIFY(row.prepare()); row.logind->holdInhibit = true; QVERIFY(row.start());
    QTRY_VERIFY(row.logind->delayedInhibit.has_value()); QTest::qWait(1000);
    const int requests = row.logind->inhibitCalls;
    row.logind->replyInhibit(); QTRY_VERIFY(!row.logind->peers.isEmpty()); QTRY_VERIFY(row.logind->allClosed());
    for (int i = 0; i < 3; ++i) { row.logind->lid(i % 2 == 0); row.source(false); QTest::qWait(80); }
    QCOMPARE(row.logind->inhibitCalls, requests); QVERIFY(row.actions().isEmpty());
}
void LidRuntimeTests::lateFdOwnerReplacementPreservesNewDescriptor() {
    LidRuntime row; QVERIFY(row.prepare()); row.logind->holdInhibit = true; QVERIFY(row.start());
    CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend"));
    QTRY_VERIFY(row.logind->delayedInhibit.has_value());
    auto old = std::move(row.logind); old->stop();
    row.logind = std::make_unique<LidLogindWire>(row.bus.open());
    row.logind->domain.setSessionTruth(true, false, false); QVERIFY(row.logind->start()); OWNED(row);
    old->replyInhibit(); QTRY_VERIFY(!old->peers.isEmpty()); QTRY_VERIFY(old->allClosed());
    QVERIFY(row.logind->hasLiveOwned()); QVERIFY(row.logind->externalAlive()); QVERIFY(old->externalAlive());
    QVERIFY(row.actions().isEmpty());
}
void LidRuntimeTests::pendingCanFence_data() {
    QTest::addColumn<QString>("boundary");
    for (const char *name : {"preferences", "reopen", "legacy", "inactive", "settings-owner"}) QTest::newRow(name) << QString::fromLatin1(name);
}
void LidRuntimeTests::pendingCanFence() {
    QFETCH(QString, boundary);
    LidRuntime row; PREPARE(row); CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("hibernate")); OWNED(row);
    row.session->holdCan = true; row.logind->lid(true); QTRY_VERIFY(row.session->delayedCan.has_value());
    if (boundary == QStringLiteral("reopen")) { row.logind->lid(false); QTRY_VERIFY(!row.power->snapshot().source.lidClosed); }
    else if (boundary == QStringLiteral("preferences")) {
        QVERIFY(row.preference(key, QStringLiteral("none"))); QTRY_COMPARE(row.settings->snapshot()->values.value(key).toString(), QStringLiteral("none"));
    } else if (boundary == QStringLiteral("legacy")) QVERIFY(row.legacyBus.registerService(QStringLiteral("org.kde.Solid.PowerManagement")));
    else if (boundary == QStringLiteral("inactive")) row.logind->activity(false);
    else row.service.reset();
    if (boundary == QStringLiteral("legacy") || boundary == QStringLiteral("inactive") || boundary == QStringLiteral("settings-owner")) QTRY_VERIFY(row.logind->allClosed());
    QTest::qWait(100);
    row.session->replyCan(); QTest::qWait(200); QVERIFY(row.actions().isEmpty());
}
void LidRuntimeTests::dispatchedUncertaintyNeverReplays() {
    LidRuntime row; PREPARE(row); CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend")); OWNED(row);
    row.session->holdAction = true; row.logind->lid(true); QTRY_COMPARE(row.actions().size(), 1);
    QVERIFY(row.sessionBus.unregisterService(QStringLiteral("org.qindaqt.Session1"))); QTRY_VERIFY(row.logind->allClosed());
    QVERIFY(row.sessionBus.registerService(QStringLiteral("org.qindaqt.Session1"))); row.session->replyAction();
    row.logind->lid(false); QTest::qWait(100); row.logind->lid(true); QTest::qWait(250);
    QCOMPARE(row.actions().size(), 1); QVERIFY(row.logind->allClosed());
}
void LidRuntimeTests::spoofedSessionInvalidationCannotRevoke() {
    LidRuntime row; PREPARE(row); CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("suspend")); OWNED(row);
    const int requests = row.logind->inhibitCalls;
    auto spoof = QDBusMessage::createSignal(row.logind->path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
    spoof.setArguments({QStringLiteral("org.freedesktop.login1.Session"), QVariantMap{{QStringLiteral("Active"), false}}, QStringList{}});
    QVERIFY(row.clientBus.send(spoof)); QTest::qWait(100);
    QVERIFY(row.logind->hasLiveOwned()); QCOMPARE(row.logind->inhibitCalls, requests);
    row.logind->lid(true); QTRY_COMPARE(row.actions().size(), 1);
}
void LidRuntimeTests::shutdownClosesEveryOwnedDuplicate() {
    LidRuntime row; PREPARE(row); CONFIGURE(row, QStringLiteral("ac"), QStringLiteral("action"), QStringLiteral("none")); OWNED(row);
    row.stopResident(); QTRY_VERIFY(row.logind->allClosed()); QVERIFY(row.logind->externalAlive());
}
QTEST_GUILESS_MAIN(LidRuntimeTests)
#include "tst_lid_runtime.moc"
