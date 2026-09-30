// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/session_lock_state/native_lock_state_monitor.h"
#include "qindaqt/services/session_lock_state/native_lock_transport.h"
#include <QtTest>
using namespace QindaQt::Services::SessionLockState;
class MemoryTransport final : public NativeLockTransport {
public:
  struct Query {
    quint64 generation, serial;
    QString owner;
  };
  bool start(QString *) override {
    started = true;
    return true;
  }
  void stop() override { started = false; }
  void requestOwner(quint64 value) override {
    generation = value;
    ++ownerRequests;
  }
  void requestPid(quint64 value, const QString &name) override {
    generation = value;
    owner = name;
  }
  bool subscribe(const QString &name) override {
    subscribed = name;
    return acceptSubscription;
  }
  void unsubscribe() override { subscribed.clear(); }
  void requestState(quint64 gen, quint64 serial, const QString &name) override {
    queryBeforeSubscription |= subscribed != name;
    queries.append({gen, serial, name});
  }
  void authority(const QString &name = QStringLiteral(":1.10"),
                 quint64 pid = 4242) {
    Q_EMIT ownerResolved(generation, name);
    Q_EMIT pidResolved(generation, name, pid);
  }
  void answer(const Query &query, bool locked, bool protectedPresentation) {
    Q_EMIT stateResolved(query.generation, query.serial, query.owner, locked,
                         protectedPresentation);
  }
  quint64 generation = 0;
  int ownerRequests = 0;
  QString owner, subscribed;
  QList<Query> queries;
  bool started = false, acceptSubscription = true,
       queryBeforeSubscription = false;
};
class NativeLockMonitorTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void missingAdmission() {
    MemoryTransport transport;
    NativeLockStateMonitor monitor(transport, {});
    QString error;
    QVERIFY(!monitor.start(&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());
    QCOMPARE(transport.ownerRequests, 0);
  }
  void protection_data() {
    QTest::addColumn<bool>("locked");
    QTest::addColumn<bool>("protectedPresentation");
    QTest::addColumn<LockState>("expected");
    QTest::newRow("unlocked") << false << false << LockState::Unlocked;
    QTest::newRow("locking") << true << false << LockState::Locking;
    QTest::newRow("physically-protected") << true << true << LockState::Locked;
    QTest::newRow("inconsistent") << false << true << LockState::Unknown;
  }
  void protection() {
    QFETCH(bool, locked);
    QFETCH(bool, protectedPresentation);
    QFETCH(LockState, expected);
    MemoryTransport transport;
    NativeLockStateMonitor monitor(transport,
                                   [](const QString &owner, quint64 pid) {
                                     return owner == ":1.10" && pid == 4242;
                                   });
    QVERIFY(monitor.start());
    transport.authority();
    QCOMPARE(transport.queries.size(), 1);
    QVERIFY(!transport.queryBeforeSubscription);
    transport.answer(transport.queries.last(), locked, protectedPresentation);
    QCOMPARE(monitor.state(), expected);
    QCOMPARE(monitor.contentMayBeShown(), expected == LockState::Unlocked);
    QCOMPARE(monitor.presentationProtected(), expected == LockState::Locked);
  }
  void samePidWrongOwner() {
    MemoryTransport transport;
    NativeLockStateMonitor monitor(transport,
                                   [](const QString &owner, quint64 pid) {
                                     return owner == ":1.10" && pid == 4242;
                                   });
    QVERIFY(monitor.start());
    transport.authority(":1.20", 4242);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(transport.queries.isEmpty());
    QVERIFY(transport.subscribed.isEmpty());
  }
  void staleUnlockedReplyCannotDisclose() {
    MemoryTransport transport;
    NativeLockStateMonitor monitor(
        transport, [](const QString &, quint64 pid) { return pid == 4242; });
    QVERIFY(monitor.start());
    transport.authority();
    const auto first = transport.queries.last();
    Q_EMIT transport.stateInvalidated(":1.10");
    const auto second = transport.queries.last();
    QVERIFY(second.serial != first.serial);
    transport.answer(first, false, false);
    QVERIFY(!monitor.contentMayBeShown());
    transport.answer(second, true, false);
    QCOMPARE(monitor.state(), LockState::Locking);
    Q_EMIT transport.stateInvalidated(":1.10");
    transport.answer(transport.queries.last(), true, true);
    QVERIFY(monitor.presentationProtected());
    transport.answer(first, false, false);
    QCOMPARE(monitor.state(), LockState::Locked);
  }
  void liveAdmissionRevoked() {
    bool live = true;
    MemoryTransport transport;
    NativeLockStateMonitor monitor(
        transport, [&](const QString &owner, quint64 pid) {
          return live && owner == ":1.10" && pid == 4242;
        });
    QVERIFY(monitor.start());
    transport.authority();
    const auto pending = transport.queries.last();
    live = false;
    transport.answer(pending, false, false);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());
    live = true;
    monitor.refresh();
    transport.authority();
    transport.answer(transport.queries.last(), false, false);
    QVERIFY(monitor.contentMayBeShown());
    live = false;
    QVERIFY(!monitor.contentMayBeShown()); // immediate getter guard, before
                                           // queued revocation
    monitor.refresh();
    QCOMPARE(monitor.state(), LockState::Unknown);
  }
  void directObserverRevokesAdmission() {
    bool live = true;
    MemoryTransport transport;
    NativeLockStateMonitor monitor(
        transport, [&](const QString &, quint64) { return live; });
    QSignalSpy content(&monitor,
                       &NativeLockStateMonitor::contentMayBeShownChanged);
    connect(&monitor, &NativeLockStateMonitor::stateChanged, &monitor,
            [&](LockState state) {
              if (state == LockState::Unlocked)
                live = false;
            });
    QVERIFY(monitor.start());
    transport.authority();
    transport.answer(transport.queries.last(), false, false);
    QVERIFY(!monitor.contentMayBeShown());
    QVERIFY(content.isEmpty());
  }
  void ownerLossAndReplacement() {
    MemoryTransport transport;
    NativeLockStateMonitor monitor(transport,
                                   [](const QString &owner, quint64 pid) {
                                     return owner == ":1.10" && pid == 4242;
                                   });
    QVERIFY(monitor.start());
    transport.authority();
    const auto old = transport.queries.last();
    transport.answer(old, false, false);
    QVERIFY(monitor.contentMayBeShown());
    Q_EMIT transport.ownerChanged();
    QCOMPARE(monitor.state(), LockState::Unknown);
    transport.answer(old, false, false);
    QVERIFY(!monitor.contentMayBeShown());
    transport.authority(":1.11", 4242);
    QCOMPARE(monitor.state(), LockState::Unknown);
    const auto count = transport.queries.size();
    Q_EMIT transport.stateInvalidated(":1.10");
    QCOMPARE(transport.queries.size(), count);
  }
  void busLossFencesEpoch() {
    MemoryTransport transport;
    NativeLockStateMonitor monitor(
        transport, [](const QString &, quint64) { return true; });
    QVERIFY(monitor.start());
    transport.authority();
    const auto old = transport.queries.last();
    transport.answer(old, true, true);
    QVERIFY(monitor.presentationProtected());
    Q_EMIT transport.lost();
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.presentationProtected());
    transport.answer(old, false, false);
    QVERIFY(!monitor.contentMayBeShown());
    QVERIFY(monitor.start());
    transport.authority();
    transport.answer(old, false, false);
    QVERIFY(!monitor.contentMayBeShown());
    transport.answer(transport.queries.last(), false, false);
    QVERIFY(monitor.contentMayBeShown());
  }
  void deniedSubscription() {
    MemoryTransport transport;
    transport.acceptSubscription = false;
    NativeLockStateMonitor monitor(
        transport, [](const QString &, quint64) { return true; });
    QVERIFY(monitor.start());
    transport.authority();
    QVERIFY(transport.queries.isEmpty());
    QCOMPARE(monitor.state(), LockState::Unknown);
  }
  void failedQueryFencesLateReply() {
    MemoryTransport transport;
    NativeLockStateMonitor monitor(
        transport, [](const QString &, quint64) { return true; });
    QVERIFY(monitor.start());
    transport.authority();
    const auto query = transport.queries.last();
    Q_EMIT transport.failed(query.generation, query.serial, query.owner,
                            "denied");
    transport.answer(query, false, false);
    QVERIFY(!monitor.contentMayBeShown());
  }
};
QTEST_GUILESS_MAIN(NativeLockMonitorTests)
#include "tst_native_lock_monitor.moc"
