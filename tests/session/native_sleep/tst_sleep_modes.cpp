// SPDX-License-Identifier: GPL-3.0-or-later
#include "sleep_test_support.h"
using namespace SleepTest;
namespace {
void modeRows() {
  QTest::addColumn<int>("value");
  for (const auto mode : {SleepMode::Suspend, SleepMode::Hibernate,
                         SleepMode::HybridSleep, SleepMode::SuspendThenHibernate})
    QTest::newRow(qPrintable(actionMethod(mode))) << static_cast<int>(mode);
}
QDBusMessage call(SleepMode mode, bool capability = false) {
  return QDBusMessage::createMethodCall(QStringLiteral("org.qindaqt.Sleep1"),
      QStringLiteral("/org/qindaqt/Sleep1"), QStringLiteral("org.qindaqt.Sleep1"),
      capability ? capabilityMethod(mode) : actionMethod(mode));
}
void booleanReply(QDBusPendingCall pending, bool expected) {
  const auto reply = waitReply(pending);
  QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
  QCOMPARE(reply.signature(), QStringLiteral("b"));
  QCOMPARE(reply.arguments().first().toBool(), expected);
}
}
class SleepModesTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void everyModeNeedsProtectedReceipt_data() { modeRows(); }
  void everyModeNeedsProtectedReceipt();
  void capabilityRefusal_data();
  void capabilityRefusal();
  void delayedProtectionLoss_data() { modeRows(); }
  void delayedProtectionLoss();
  void missingCurrentNonce_data() { modeRows(); }
  void missingCurrentNonce();
  void dispatchedTimeout_data() { modeRows(); }
  void dispatchedTimeout();
  void actualCallerUid_data() { modeRows(); }
  void actualCallerUid();
  void staleCapabilityAndNewMode_data() { modeRows(); }
  void staleCapabilityAndNewMode();
  void ownerLoss_data();
  void ownerLoss();
  void capabilityIsNotReservation_data() { modeRows(); }
  void capabilityIsNotReservation();
  void malformedActionIsUncertain_data() { modeRows(); }
  void malformedActionIsUncertain();
  void unknownAndLockingDeny_data() { modeRows(); }
  void unknownAndLockingDeny();
  void stoppedReadonlyQueryRepliesFalse();
  void invalidModeAndWireCannotDispatch();
};
void SleepModesTests::everyModeNeedsProtectedReceipt() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start();
  booleanReply(f.attacker.asyncCall(call(mode, true)), true);
  QCOMPARE(f.logind.capabilityMethods, QStringList{capabilityMethod(mode)});
  QCOMPARE(f.native.requests, 0); QVERIFY(f.logind.actionMethods.isEmpty());
  auto pending = f.attacker.asyncCall(call(mode), 5000);
  QTRY_COMPARE(f.native.requests, 1);
  // No logind mutation or dispatch capability check before current protection.
  QCOMPARE(f.logind.canCalls, 1); QVERIFY(f.logind.actionMethods.isEmpty());
  booleanReply(f.attacker.asyncCall(call(SleepMode::Hibernate)), false);
  booleanReply(f.attacker.asyncCall(call(mode, true)), false);
  f.native.state(true, false); QTRY_COMPARE(f.monitor.state(), LockState::Locking);
  QVERIFY(f.logind.actionMethods.isEmpty()); QVERIFY(!f.logind.inhibitorClosed());
  f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  booleanReply(pending, true);
  QCOMPARE(f.logind.capabilityMethods, (QStringList{capabilityMethod(mode), capabilityMethod(mode)}));
  QCOMPARE(f.logind.actionMethods, QStringList{actionMethod(mode)});
  QVERIFY(!f.logind.interactive); QVERIFY(!f.logind.inhibitorClosed());
  f.coordinator.stop(); QTRY_VERIFY(f.logind.inhibitorClosed());
}
void SleepModesTests::capabilityRefusal_data() {
  QTest::addColumn<int>("value"); QTest::addColumn<QString>("answer");
  for (const auto mode : {SleepMode::Suspend, SleepMode::Hibernate,
                         SleepMode::HybridSleep, SleepMode::SuspendThenHibernate})
    for (const auto *answer : {"no", "na", "challenge", "unexpected", "wrong-type", "missing"})
      QTest::newRow(qPrintable(actionMethod(mode) + u'-' + QString::fromLatin1(answer)))
          << static_cast<int>(mode) << QString::fromLatin1(answer);
}
void SleepModesTests::capabilityRefusal() {
  QFETCH(int, value); QFETCH(QString, answer); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start();
  if (answer == "missing") f.logind.unsupportedMethods.append(capabilityMethod(mode));
  else f.logind.capabilityAnswers.insert(capabilityMethod(mode),
      answer == "wrong-type" ? QVariant(true) : QVariant(answer));
  booleanReply(f.attacker.asyncCall(call(mode, true)), false);
  f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  booleanReply(f.attacker.asyncCall(call(mode)), false);
  QVERIFY(f.logind.actionMethods.isEmpty()); QVERIFY(!f.logind.inhibitorClosed());
}
void SleepModesTests::delayedProtectionLoss() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  f.logind.deferCan = true;
  auto pending = f.attacker.asyncCall(call(mode), 5000); QTRY_COMPARE(f.logind.canCalls, 1);
  f.native.state(true, false); QTRY_COMPARE(f.monitor.state(), LockState::Locking);
  f.logindBus.send(f.logind.deferredCan.createReply(QStringLiteral("yes")));
  booleanReply(pending, false); QVERIFY(f.logind.actionMethods.isEmpty());
  QVERIFY(!f.logind.inhibitorClosed()); f.coordinator.stop(); QTRY_VERIFY(f.logind.inhibitorClosed());
}
void SleepModesTests::missingCurrentNonce() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start();
  auto pending = f.attacker.asyncCall(call(mode), 5000); QTRY_COMPARE(f.native.requests, 1);
  f.native.wrongNonce = true; f.native.state(true, true); QTest::qWait(40);
  QVERIFY(!f.monitor.presentationProtected()); QCOMPARE(f.logind.canCalls, 0);
  QVERIFY(f.logind.actionMethods.isEmpty()); QVERIFY(!f.logind.inhibitorClosed());
  f.service.stop(); booleanReply(pending, false); QTRY_VERIFY(f.logind.inhibitorClosed());
}
void SleepModesTests::dispatchedTimeout() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  f.logind.deferSuspend = true;
  auto pending = f.attacker.asyncCall(call(mode), 5000); QTRY_COMPARE(f.logind.suspendCalls, 1);
  const auto reply = waitReply(pending);
  QCOMPARE(reply.errorName(), QStringLiteral("org.qindaqt.Sleep1.Uncertain"));
  f.logindBus.send(f.logind.deferredSuspend.createReply()); QTest::qWait(30);
  QCOMPARE(f.logind.actionMethods, QStringList{actionMethod(mode)});
}
void SleepModesTests::actualCallerUid() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); f.service.stop();
  SleepService denied(f.supervisor, f.coordinator, static_cast<quint32>(getuid() + 1));
  QVERIFY(denied.start());
  booleanReply(f.attacker.asyncCall(call(mode, true)), false);
  booleanReply(f.attacker.asyncCall(call(mode)), false);
  QCOMPARE(f.logind.canCalls, 0); QCOMPARE(f.native.requests, 0);
}
void SleepModesTests::staleCapabilityAndNewMode() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  f.logind.deferCan = true;
  auto old = f.attacker.asyncCall(call(mode), 5000); QTRY_COMPARE(f.logind.canCalls, 1);
  const auto stale = f.logind.deferredCan;
  f.logind.prepare(false); booleanReply(old, false);
  // New explicit request may proceed; the retired Can reply cannot dispatch it.
  auto next = f.attacker.asyncCall(call(SleepMode::Hibernate), 5000);
  QTRY_COMPARE(f.logind.canCalls, 2);
  f.logindBus.send(stale.createReply(QStringLiteral("yes"))); QTest::qWait(30);
  QVERIFY(f.logind.actionMethods.isEmpty()); QVERIFY(!next.isFinished());
  f.logindBus.send(f.logind.deferredCan.createReply(QStringLiteral("yes")));
  booleanReply(next, true); QCOMPARE(f.logind.actionMethods, QStringList{QStringLiteral("Hibernate")});
}
void SleepModesTests::ownerLoss_data() {
  QTest::addColumn<int>("value"); QTest::addColumn<bool>("dispatched");
  for (const auto mode : {SleepMode::Suspend, SleepMode::Hibernate,
                         SleepMode::HybridSleep, SleepMode::SuspendThenHibernate})
    for (bool dispatched : {false, true})
      QTest::newRow(qPrintable(actionMethod(mode) + (dispatched ? "-after" : "-before")))
          << static_cast<int>(mode) << dispatched;
}
void SleepModesTests::ownerLoss() {
  QFETCH(int, value); QFETCH(bool, dispatched); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  f.logind.deferCan = !dispatched; f.logind.deferSuspend = dispatched;
  auto pending = f.attacker.asyncCall(call(mode), 5000);
  QTRY_COMPARE(f.logind.canCalls, 1);
  if (dispatched) QTRY_COMPARE(f.logind.suspendCalls, 1);
  f.logindBus.unregisterService(QStringLiteral("org.freedesktop.login1"));
  QTRY_VERIFY(f.logind.inhibitorClosed());
  const auto reply = waitReply(pending);
  if (dispatched) QCOMPARE(reply.errorName(), QStringLiteral("org.qindaqt.Sleep1.Uncertain"));
  else { QCOMPARE(reply.signature(), QStringLiteral("b")); QVERIFY(!reply.arguments().first().toBool()); }
  f.logindBus.send((dispatched ? f.logind.deferredSuspend : f.logind.deferredCan)
      .createReply(dispatched ? QVariantList{} : QVariantList{QStringLiteral("yes")}));
  QTest::qWait(30); QCOMPARE(f.logind.suspendCalls, dispatched ? 1 : 0);
}
void SleepModesTests::capabilityIsNotReservation() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); booleanReply(f.attacker.asyncCall(call(mode, true)), true);
  f.logind.capabilityAnswers[capabilityMethod(mode)] = QStringLiteral("no");
  auto pending = f.attacker.asyncCall(call(mode), 5000);
  QTRY_COMPARE(f.native.requests, 1); f.native.state(true, true);
  booleanReply(pending, false); QCOMPARE(f.logind.canCalls, 2); QVERIFY(f.logind.actionMethods.isEmpty());
}
void SleepModesTests::malformedActionIsUncertain() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  f.logind.malformedActionReply = true;
  const auto reply = waitReply(f.attacker.asyncCall(call(mode), 5000));
  QCOMPARE(reply.errorName(), QStringLiteral("org.qindaqt.Sleep1.Uncertain"));
  QCOMPARE(f.logind.actionMethods, QStringList{actionMethod(mode)});
}
void SleepModesTests::unknownAndLockingDeny() {
  QFETCH(int, value); const auto mode = static_cast<SleepMode>(value);
  Fixture f; f.start(); f.native.state(true, false); QTRY_COMPARE(f.monitor.state(), LockState::Locking);
  booleanReply(f.attacker.asyncCall(call(mode, true)), false);
  booleanReply(f.attacker.asyncCall(call(mode)), false);
  f.native.omitReceipt = true; f.monitor.refresh();
  booleanReply(f.attacker.asyncCall(call(mode)), false);
  QCOMPARE(f.logind.canCalls, 0); QVERIFY(f.logind.actionMethods.isEmpty());
}
void SleepModesTests::stoppedReadonlyQueryRepliesFalse() {
  Fixture f; f.start(); f.logind.deferCan = true;
  auto pending = f.attacker.asyncCall(call(SleepMode::Hibernate, true), 5000);
  QTRY_COMPARE(f.logind.canCalls, 1); f.service.stop(); booleanReply(pending, false);
  f.logindBus.send(f.logind.deferredCan.createReply(QStringLiteral("yes")));
  QTest::qWait(30); QVERIFY(f.logind.actionMethods.isEmpty());
}
void SleepModesTests::invalidModeAndWireCannotDispatch() {
  Fixture f; f.start(); QVERIFY(!f.coordinator.requestSleep(static_cast<SleepMode>(999)));
  const auto xml = f.service.introspect(QStringLiteral("/org/qindaqt/Sleep1"));
  for (const auto mode : {SleepMode::Suspend, SleepMode::Hibernate,
                         SleepMode::HybridSleep, SleepMode::SuspendThenHibernate}) {
    QVERIFY(xml.contains(QStringLiteral("name=\"%1\"").arg(actionMethod(mode))));
    QVERIFY(xml.contains(QStringLiteral("name=\"%1\"").arg(capabilityMethod(mode))));
  }
  auto message = call(SleepMode::Hibernate); message.setArguments({true});
  const auto reply = waitReply(f.attacker.asyncCall(message));
  QCOMPARE(reply.type(), QDBusMessage::ErrorMessage); QCOMPARE(f.logind.canCalls, 0);
}
QTEST_GUILESS_MAIN(SleepModesTests)
#include "tst_sleep_modes.moc"
