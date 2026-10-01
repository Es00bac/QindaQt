// SPDX-License-Identifier: GPL-3.0-or-later
#include "sleep_test_support.h"
using namespace SleepTest;
class LogindSleepTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void selectedIdentityAndInhibitWireAreExact();
  void foreignOrMalformedSessionIsRefused_data();
  void foreignOrMalformedSessionIsRefused();
  void daemonUidIsRequired();
  void stopAndRestartCloseDescriptor();
  void stoppedGenerationClosesLateDescriptor();
  void peerLossClosesDescriptorBeforeAnyMutation();
  void ownerReplacementClosesAndRevalidates();
  void foreignSignalsAreIgnored();
  void delayedCanCannotBypassProtection();
  void selectedSessionRemovalRevokesDescriptor();
  void startingDuringSleepNeverAcquiresDelay();
};
void LogindSleepTests::selectedIdentityAndInhibitWireAreExact() {
  Fixture f; f.start();
  QCOMPARE(f.logind.selectedIds, QStringList{"native1"});
  QCOMPARE(f.logind.selectedPids, QList<quint32>{static_cast<quint32>(getpid())});
  QCOMPARE(f.logind.inhibitors.size(), 1);
  QCOMPARE(f.logind.inhibitors.first().at(0).toString(), QStringLiteral("sleep"));
  QCOMPARE(f.logind.inhibitors.first().at(3).toString(), QStringLiteral("delay"));
  QVERIFY(!f.logind.inhibitorClosed());
}
void LogindSleepTests::foreignOrMalformedSessionIsRefused_data() {
  QTest::addColumn<QString>("kind");
  for (const auto *kind : {"pid", "id", "uid", "user", "path"}) QTest::newRow(kind) << QString(kind);
}
void LogindSleepTests::foreignOrMalformedSessionIsRefused() {
  QFETCH(QString, kind);
  Fixture f; f.startNative();
  if (kind == "pid") f.logind.pidPath += "wrong";
  if (kind == "id") f.logind.id = "foreign";
  if (kind == "uid") ++f.logind.uid;
  if (kind == "user") f.logind.malformedUser = true;
  if (kind == "path") f.logind.path = "/foreign/session";
  f.logind.claim(); f.coordinator.start();
  QTest::qWait(200);
  QVERIFY(!f.transport.available()); QVERIFY(!f.coordinator.requestSuspend());
  QVERIFY(f.logind.inhibitors.isEmpty()); QCOMPARE(f.logind.suspendCalls, 0);
}
void LogindSleepTests::daemonUidIsRequired() {
  Fixture f; f.startNative(); f.logind.claim();
  LogindSleepTransport transport(f.supervisor, "native1", static_cast<quint32>(getuid()),
      static_cast<quint32>(getpid()), [] { return true; }, static_cast<quint32>(getuid() + 1));
  transport.start(); QTest::qWait(200); QVERIFY(!transport.available());
  QVERIFY(f.logind.inhibitors.isEmpty());
}
void LogindSleepTests::stopAndRestartCloseDescriptor() {
  Fixture f; f.start();
  f.coordinator.stop(); QTRY_VERIFY(f.logind.inhibitorClosed());
  f.coordinator.start(); QTRY_VERIFY(f.transport.hasDelayInhibitor());
  QCOMPARE(f.logind.readEnds.size(), 2); QVERIFY(!f.logind.inhibitorClosed(1));
  f.coordinator.stop(); QTRY_VERIFY(f.logind.inhibitorClosed(1));
}
void LogindSleepTests::stoppedGenerationClosesLateDescriptor() {
  Fixture f; f.startNative(); f.logind.deferInhibit = true; f.logind.claim(); f.coordinator.start();
  QTRY_VERIFY(f.logind.deferredInhibit.type() == QDBusMessage::MethodCallMessage);
  f.coordinator.stop(); f.logind.sendInhibit(f.logind.deferredInhibit);
  QTRY_VERIFY(f.logind.inhibitorClosed()); QVERIFY(!f.transport.hasDelayInhibitor());
}
void LogindSleepTests::peerLossClosesDescriptorBeforeAnyMutation() {
  Fixture f; f.start(); f.supervisorAdmitted = false;
  QVERIFY(!f.transport.hasDelayInhibitor()); QVERIFY(!f.coordinator.requestSuspend());
  QTRY_VERIFY(f.logind.inhibitorClosed()); QCOMPARE(f.logind.suspendCalls, 0);
}
void LogindSleepTests::ownerReplacementClosesAndRevalidates() {
  Fixture f; f.start(); f.logindBus.unregisterService("org.freedesktop.login1");
  QTRY_VERIFY(f.logind.inhibitorClosed()); QVERIFY(!f.transport.available());
  auto replacementBus = f.broker.connect("replacement"); FakeLogind replacement(replacementBus);
  replacement.uid++; replacement.claim(); QTest::qWait(200);
  QVERIFY(!f.transport.available()); QVERIFY(replacement.inhibitors.isEmpty());
}
void LogindSleepTests::foreignSignalsAreIgnored() {
  Fixture f; f.start(); QSignalSpy lock(&f.transport, &LogindSleepTransport::lockRequested);
  QSignalSpy prepare(&f.transport, &LogindSleepTransport::prepareForSleep);
  f.logind.signal("Lock", f.logind.path, {}, &f.attacker);
  f.logind.prepare(true, &f.attacker); QTest::qWait(100);
  QCOMPARE(lock.size(), 0); QCOMPARE(prepare.size(), 0); QVERIFY(!f.logind.inhibitorClosed());
}
void LogindSleepTests::delayedCanCannotBypassProtection() {
  Fixture f; f.start(); f.logind.deferCan = true;
  f.native.state(true, true); QTRY_VERIFY(f.monitor.presentationProtected());
  QVERIFY(f.coordinator.requestSuspend()); QTRY_COMPARE(f.logind.canCalls, 1);
  f.supervisorAdmitted = false;
  f.logindBus.send(f.logind.deferredCan.createReply(QStringLiteral("yes")));
  QTest::qWait(100); QCOMPARE(f.logind.suspendCalls, 0);
  QTRY_VERIFY(f.logind.inhibitorClosed());
}
void LogindSleepTests::selectedSessionRemovalRevokesDescriptor() {
  Fixture f; f.start();
  f.logind.signal(QStringLiteral("SessionRemoved"), QStringLiteral("/org/freedesktop/login1"),
      {f.logind.id, QVariant::fromValue(QDBusObjectPath(f.logind.path))});
  QTRY_VERIFY(f.logind.inhibitorClosed()); QVERIFY(!f.transport.available());
}
void LogindSleepTests::startingDuringSleepNeverAcquiresDelay() {
  Fixture f; f.startNative(); f.logind.preparing = true; f.logind.claim(); f.coordinator.start();
  QTRY_VERIFY(f.transport.preparingForSleep()); QVERIFY(!f.transport.hasDelayInhibitor());
  QVERIFY(!f.coordinator.requestSuspend()); QVERIFY(f.logind.inhibitors.isEmpty());
  f.logind.prepare(false); QTRY_VERIFY(f.transport.hasDelayInhibitor());
}
QTEST_GUILESS_MAIN(LogindSleepTests)
#include "tst_logind_sleep_transport.moc"
