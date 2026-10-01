// SPDX-License-Identifier: GPL-3.0-or-later
#include "sleep_test_support.h"
using namespace SleepTest;
class SleepCoordinatorTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void manualSuspendRequiresTargetedProtectedReceipt();
  void systemPrepareRetainsDelayUntilProtectedThenRearms();
  void failedAdmissionNeverReleasesDelay();
  void forgedStateNonceCannotReleaseDelay();
  void unlockSignalDoesNotAuthenticate();
  void lockedHintUsesOnlyAuthenticatedState();
  void unknownOrLockingManualSleepIsRefused();
  void supervisorLossRevokesFacadeAndDescriptor();
  void facadeReturnsConclusiveResult();
  void selectedLockSignalRequestsNativeAdmission();
  void manualPrepareRaceStillReleasesOnlyProtected();
};
void SleepCoordinatorTests::manualSuspendRequiresTargetedProtectedReceipt() {
  Fixture f; f.start(); QSignalSpy result(&f.coordinator, &SleepCoordinator::suspendFinished);
  QVERIFY(f.coordinator.requestSuspend()); QTRY_COMPARE(f.native.requests, 1);
  QCOMPARE(f.logind.suspendCalls, 0); QVERIFY(!f.logind.inhibitorClosed());
  f.native.state(true, false); QTRY_COMPARE(f.monitor.state(), LockState::Locking);
  QCOMPARE(f.logind.suspendCalls, 0);
  f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  QTRY_COMPARE(f.logind.suspendCalls, 1); QVERIFY(!f.logind.interactive);
  QTRY_COMPARE(result.size(), 1); QCOMPARE(qvariant_cast<SleepResult>(result.first().first()), SleepResult::Confirmed);
}
void SleepCoordinatorTests::systemPrepareRetainsDelayUntilProtectedThenRearms() {
  Fixture f; f.start(); f.logind.prepare(true);
  QTRY_COMPARE(f.native.requests, 1); QVERIFY(!f.logind.inhibitorClosed());
  f.native.state(true, false); QTRY_COMPARE(f.monitor.state(), LockState::Locking);
  QVERIFY(!f.logind.inhibitorClosed());
  f.native.state(true, true); QTRY_VERIFY(f.logind.inhibitorClosed());
  f.logind.prepare(false); QTRY_COMPARE(f.logind.readEnds.size(), 2);
  QTRY_VERIFY(f.transport.hasDelayInhibitor()); QVERIFY(!f.logind.inhibitorClosed(1));
}
void SleepCoordinatorTests::failedAdmissionNeverReleasesDelay() {
  Fixture f; f.start(); f.native.admit = false; f.logind.prepare(true);
  QTRY_COMPARE(f.native.requests, 1); QTest::qWait(100);
  QVERIFY(!f.logind.inhibitorClosed()); QCOMPARE(f.logind.suspendCalls, 0);
  f.coordinator.stop(); QTRY_VERIFY(f.logind.inhibitorClosed());
}
void SleepCoordinatorTests::forgedStateNonceCannotReleaseDelay() {
  Fixture f; f.start(); f.native.wrongNonce = true; f.logind.prepare(true);
  QTRY_COMPARE(f.native.requests, 1); f.native.state(true, true);
  QTest::qWait(200); QVERIFY(!f.monitor.presentationProtected());
  QVERIFY(!f.logind.inhibitorClosed()); QCOMPARE(f.logind.suspendCalls, 0);
}
void SleepCoordinatorTests::unlockSignalDoesNotAuthenticate() {
  Fixture f; f.start(); f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  f.logind.signal("Unlock", f.logind.path); QTest::qWait(100);
  QCOMPARE(f.monitor.state(), LockState::Locked); QVERIFY(f.monitor.presentationProtected());
}
void SleepCoordinatorTests::lockedHintUsesOnlyAuthenticatedState() {
  Fixture f; f.start(); QTRY_VERIFY(f.logind.hints.contains(false));
  f.native.state(true, false); QTRY_COMPARE(f.monitor.state(), LockState::Locking);
  const auto count = f.logind.hints.size();
  QTest::qWait(100); QCOMPARE(f.logind.hints.size(), count);
  f.native.state(true, true); QTRY_VERIFY(f.logind.hints.contains(true));
}
void SleepCoordinatorTests::unknownOrLockingManualSleepIsRefused() {
  Fixture f; f.start(); f.native.state(true, false); QTRY_COMPARE(f.monitor.state(), LockState::Locking);
  QVERIFY(!f.coordinator.canSuspend()); QVERIFY(!f.coordinator.requestSuspend());
  f.native.omitReceipt = true; f.monitor.refresh();
  QVERIFY(!f.coordinator.canSuspend()); QVERIFY(!f.coordinator.requestSuspend());
  QCOMPARE(f.logind.suspendCalls, 0);
}
void SleepCoordinatorTests::supervisorLossRevokesFacadeAndDescriptor() {
  Fixture f; f.start(); f.supervisor.unregisterService("org.qindaqt.Session1");
  QTRY_VERIFY(f.logind.inhibitorClosed()); QVERIFY(!f.transport.hasDelayInhibitor());
  QTRY_VERIFY(!f.supervisor.interface()->isServiceRegistered("org.qindaqt.Sleep1").value());
}
void SleepCoordinatorTests::facadeReturnsConclusiveResult() {
  Fixture f; f.start();
  auto call = QDBusMessage::createMethodCall(QStringLiteral("org.qindaqt.Sleep1"),
      QStringLiteral("/org/qindaqt/Sleep1"), QStringLiteral("org.qindaqt.Sleep1"), QStringLiteral("Suspend"));
  auto pending = f.attacker.asyncCall(call, 5000);
  QTRY_COMPARE(f.native.requests, 1); QVERIFY(!pending.isFinished());
  f.native.state(true, true);
  const auto reply = waitReply(pending);
  QCOMPARE(reply.type(), QDBusMessage::ReplyMessage); QCOMPARE(reply.signature(), QStringLiteral("b"));
  QVERIFY(reply.arguments().first().toBool()); QCOMPARE(f.logind.suspendCalls, 1);
}
void SleepCoordinatorTests::selectedLockSignalRequestsNativeAdmission() {
  Fixture f; f.start(); f.logind.signal(QStringLiteral("Lock"), f.logind.path);
  QTRY_COMPARE(f.native.requests, 1); QCOMPARE(f.logind.suspendCalls, 0);
}
void SleepCoordinatorTests::manualPrepareRaceStillReleasesOnlyProtected() {
  Fixture f; f.start(); f.logind.autoPrepare = true;
  QVERIFY(f.coordinator.requestSuspend()); QTRY_COMPARE(f.native.requests, 1);
  f.native.state(true, true); QTRY_COMPARE(f.logind.suspendCalls, 1);
  QTRY_VERIFY(f.logind.inhibitorClosed());
}
QTEST_GUILESS_MAIN(SleepCoordinatorTests)
#include "tst_sleep_coordinator.moc"
