// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

#include <QtTest>

#include "task_list_producer_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

namespace {

// Scriptable in-process transport: no bus, no timers of its own. The test
// drives replies and signals explicitly so every fencing race is exact.
class FakeProducerTransport final : public TaskListProducerTransport {
  Q_OBJECT

public:
  bool start(QString *error = nullptr) override {
    if (error) {
      error->clear();
    }
    started = true;
    return true;
  }
  void stop() override { started = false; }

  void requestRefresh(quint64 token, const QString &uniqueOwner) override {
    requestedTokens.append(token);
    requestedOwners.append(uniqueOwner);
  }

  void emitOwner(const QString &owner) { Q_EMIT serviceOwnerChanged(owner); }
  void emitInvalidation(const QString &owner) {
    Q_EMIT refreshInvalidated(owner);
  }
  void emitScene(quint64 token, const QString &owner,
                 const StandardScene &scene) {
    Q_EMIT windowsRead(token, owner, scene.windows);
    Q_EMIT containersRead(token, owner, scene.containers);
    Q_EMIT scopeRead(token, owner, scene.scope);
  }

  bool started = false;
  QVector<quint64> requestedTokens;
  QStringList requestedOwners;
};

TaskListFactsProducerTiming fastTiming() {
  TaskListFactsProducerTiming timing;
  timing.debounceMilliseconds = 1;
  timing.requestTimeoutMilliseconds = 80;
  timing.retryMilliseconds = {5, 10, 20};
  return timing;
}

} // namespace

class TaskListFactsProducerTests final : public QObject {
  Q_OBJECT

private slots:
  void publishesCoherentGeneration();
  void discardsRefreshRacedByInvalidation();
  void malformedReplyDegradesAndRetainsGeneration();
  void timeoutDegradesAndRetries();
  void ownerLossDegradesAndFencesLateReplies();
  void ownerReplacementRebindsReads();
  void invalidFactsRejectedBySourceDegrade();
  void stopIsSafeDuringRefresh();
  void tornScopeFenceRereadsOnceThenDegrades();
  void regressedScopeRevisionIsRejected();
  void coherentEpochReplacementPublishes();
  void degradationPublishesStateChanged();
  void joinFailurePublishesStateChanged();
  void stopWithdrawsAvailability();
  void thousandsOfWindowsPublish();
};

void TaskListFactsProducerTests::publishesCoherentGeneration() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
  QString error;
  QVERIFY2(producer.start(&error), qPrintable(error));
  QCOMPARE(source.status(), TaskListSourceStatus::Loading);

  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  QCOMPARE(transport.requestedOwners.constFirst(), QStringLiteral(":1.1"));
  QCOMPARE(producer.uniqueOwner(), QStringLiteral(":1.1"));

  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(1));
  QCOMPARE(source.generation().entries.size(), 2);
  QVERIFY(stateSpy.size() >= 1);

  const auto lineage = producer.containerLineage(QStringLiteral("c1"));
  QVERIFY(lineage.has_value());
  QCOMPARE(lineage->revision, quint64(7));
  QCOMPARE(lineage->authority, TaskListContainerAuthority::HybridProcess);
  QVERIFY(!producer.containerLineage(QStringLiteral("nope")).has_value());
}

void TaskListFactsProducerTests::discardsRefreshRacedByInvalidation() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);

  // A change racing the three reads must fence the whole generation.
  transport.emitInvalidation(QStringLiteral(":1.1"));
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Loading);
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(1));

  // An invalidation with no in-flight refresh triggers exactly one debounced
  // re-read, no matter how often it fires.
  transport.emitInvalidation(QStringLiteral(":1.1"));
  transport.emitInvalidation(QStringLiteral(":1.1"));
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 3, 2'000);
  QTest::qWait(20);
  QCOMPARE(transport.requestedTokens.size(), 3);
}

void TaskListFactsProducerTests::malformedReplyDegradesAndRetainsGeneration() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  const TaskGeneration retained = source.generation();

  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  Q_EMIT transport.windowsRead(transport.requestedTokens.at(1),
                               QStringLiteral(":1.1"),
                               QByteArrayLiteral("{malformed"));
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  // The last accepted generation is retained, intents are refused.
  QCOMPARE(source.generation(), retained);
  TaskIntentOutcome outcome = source.requestIntent(
      {QStringLiteral("w1"), TaskIntentKind::Activate, quint64(1)});
  QCOMPARE(outcome.code, TaskIntentErrorCode::SourceDegraded);
  QVERIFY(!producer.lastError().isEmpty());

  // The bounded retry recovers once coherent data returns.
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 3, 2'000);
  transport.emitScene(transport.requestedTokens.at(2), QStringLiteral(":1.1"),
                      standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(2));
}

void TaskListFactsProducerTests::timeoutDegradesAndRetries() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);

  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  // No replies arrive: the request timeout must degrade, then retry.
  QTRY_VERIFY_WITH_TIMEOUT(transport.requestedTokens.size() >= 3, 2'000);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);

  // A late reply for the abandoned token is fenced out.
  Q_EMIT transport.windowsRead(transport.requestedTokens.at(1),
                               QStringLiteral(":1.1"),
                               standardScene().windows);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
}

void TaskListFactsProducerTests::ownerLossDegradesAndFencesLateReplies() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  const TaskGeneration retained = source.generation();

  transport.emitOwner({});
  QCOMPARE(producer.uniqueOwner(), QString());
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(source.generation(), retained);

  // Signals and replies naming the dead owner are ignored.
  transport.emitInvalidation(QStringLiteral(":1.1"));
  transport.emitScene(quint64(99), QStringLiteral(":1.1"), standardScene());
  QTest::qWait(30);
  QCOMPARE(transport.requestedTokens.size(), 1);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
}

void TaskListFactsProducerTests::ownerReplacementRebindsReads() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);

  transport.emitOwner(QStringLiteral(":1.2"));
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  QCOMPARE(transport.requestedOwners.at(1), QStringLiteral(":1.2"));

  // The replacement's reads re-publish under the new lineage.
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.2"),
                      standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(2));
}

void TaskListFactsProducerTests::invalidFactsRejectedBySourceDegrade() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);

  // Two active windows pass the wire decoders but violate the T0 batch
  // contract; the source's atomic rejection must surface as Degraded.
  StandardScene scene;
  scene.windows = windowsPayload(
      {windowJson(QStringLiteral("w1"), QStringLiteral("app.one"), {}, true),
       windowJson(QStringLiteral("w9"), QStringLiteral("app.nine"), {}, true)});
  scene.containers = containersPayload({});
  scene.scope = scopePayload(
      {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w9"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")})});
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), scene);
  // Loading is retained because no generation was ever accepted.
  QCOMPARE(source.status(), TaskListSourceStatus::Loading);
  QCOMPARE(source.revision(), quint64(0));

  // The bounded retry then admits the coherent standard scene at the next
  // revision (the compositor advances the generation when state changes; the
  // same revision with changed bytes would be a lineage collision).
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      standardScene(2));
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
}

void TaskListFactsProducerTests::stopIsSafeDuringRefresh() {
  TaskListSource source;
  FakeProducerTransport transport;
  {
    TaskListFactsProducer producer(transport, source, fastTiming());
    QVERIFY(producer.start());
    transport.emitOwner(QStringLiteral(":1.1"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
    producer.stop();
    QVERIFY(!transport.started);
    transport.emitScene(transport.requestedTokens.constFirst(),
                        QStringLiteral(":1.1"), standardScene());
  }
  QCOMPARE(source.status(), TaskListSourceStatus::Loading);
}

// AGENT-NOTE: Review finding P1-1 (rejected candidate 3a5ae17): a scope
// snapshot whose (epoch, revision) does not exactly match the schema-2
// Windows() fence is torn truth. The producer must discard it, re-read once,
// and degrade fail-closed with notification if the mismatch persists — never
// publish foreign output truth.
void TaskListFactsProducerTests::tornScopeFenceRereadsOnceThenDegrades() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(1));
  const TaskGeneration retained = source.generation();
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);

  // Scope at revision 2 while the Windows() fence still names revision 1.
  StandardScene torn = standardScene();
  torn.scope = scopePayload(
      {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w2"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w3"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")})},
      2);
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      torn);
  // Discarded without publishing; exactly one re-read follows.
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(1));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 3, 2'000);

  // A persistently torn fence degrades fail-closed and notifies.
  transport.emitScene(transport.requestedTokens.at(2), QStringLiteral(":1.1"),
                      torn);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(source.generation(), retained);
  QVERIFY(stateSpy.size() >= 1);
}

// AGENT-NOTE: Review finding P1-1 (rejected candidate 3a5ae17): under one
// owner and epoch, a scope revision that regresses below (or collides with
// changed bytes at) the accepted lineage is foreign truth and must degrade,
// not publish.
void TaskListFactsProducerTests::regressedScopeRevisionIsRejected() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);

  // Advance the accepted lineage to revision 2.
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      standardScene(2));
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(2));
  const TaskGeneration retained = source.generation();
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);

  // A coherent-fence refresh at the regressed revision 1 is rejected.
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 3, 2'000);
  transport.emitScene(transport.requestedTokens.at(2), QStringLiteral(":1.1"),
                      standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(source.revision(), quint64(2));
  QCOMPARE(source.generation(), retained);
  QVERIFY(stateSpy.size() >= 1);
}

// Positive control for the lineage fence: a coherent epoch replacement under
// the same owner (compositor instance restart) must still publish.
void TaskListFactsProducerTests::coherentEpochReplacementPublishes() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);

  const QString newEpoch =
      QStringLiteral("bbbbbbbb-3ee6-4cc5-bf64-3d46cab972d0");
  StandardScene restarted;
  restarted.windows = windowsPayload(
      {windowJson(QStringLiteral("w1"), QStringLiteral("app.one"))}, 1,
      newEpoch);
  restarted.containers = containersPayload({});
  restarted.scope = scopePayload(
      {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")})},
      1, newEpoch);
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      restarted);
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(2));
  QCOMPARE(source.generation().entries.size(), 1);
}

// AGENT-NOTE: Review finding P1-2 (rejected candidate 3a5ae17): failRefresh()
// degraded the source without emitting the only producer notification. Every
// observable degradation must publish fail-closed availability through
// stateChanged.
void TaskListFactsProducerTests::degradationPublishesStateChanged() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  const TaskGeneration retained = source.generation();
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);

  // failRefresh path: a malformed scope reply degrades and must notify.
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  StandardScene broken = standardScene(2);
  broken.scope = QByteArrayLiteral(
      "{\"status\":\"ok\",\"schemaVersion\":1,\"revision\":\"2\",\"windows\":[]}");
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      broken);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(source.generation(), retained);
  QVERIFY(stateSpy.size() >= 1);
}

// AGENT-NOTE: Review finding P1-2 (rejected candidate 3a5ae17): the
// join-failure path had the same missing notification as failRefresh().
void TaskListFactsProducerTests::joinFailurePublishesStateChanged() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  const TaskGeneration retained = source.generation();
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);

  // Two active windows pass the decoders but violate the T0 batch contract.
  StandardScene hostile = standardScene(2);
  hostile.windows = windowsPayload(
      {windowJson(QStringLiteral("w1"), QStringLiteral("app.one"), {}, true),
       windowJson(QStringLiteral("w9"), QStringLiteral("app.nine"), {}, true)},
      2);
  hostile.containers = containersPayload({});
  hostile.scope = scopePayload(
      {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w9"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")})},
      2);
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      hostile);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(source.generation(), retained);
  QVERIFY(stateSpy.size() >= 1);
}

// AGENT-NOTE: Review finding P1-2 (rejected candidate 3a5ae17): stop() left a
// Ready source, the bound owner, and container lineage live, so the operation
// adapter kept admitting mutations. Stop must withdraw owner-bound truth,
// degrade fail-closed, and notify.
void TaskListFactsProducerTests::stopWithdrawsAvailability() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  const TaskGeneration retained = source.generation();
  QVERIFY(producer.containerLineage(QStringLiteral("c1")).has_value());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);

  producer.stop();
  QCOMPARE(stateSpy.size(), 1);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(producer.uniqueOwner(), QString());
  QVERIFY(!producer.containerLineage(QStringLiteral("c1")).has_value());
  // The last accepted generation stays visible; nothing new can publish.
  QCOMPARE(source.generation(), retained);
  transport.emitScene(quint64(99), QStringLiteral(":1.1"), standardScene());
  QTest::qWait(30);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
}

// AGENT-NOTE: Review finding P2-1 (rejected candidate 3a5ae17): the at-limit
// 4,096-window scene must be a registered producer row, not a scratch check.
void TaskListFactsProducerTests::thousandsOfWindowsPublish() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  transport.emitOwner(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 1, 2'000);
  transport.emitScene(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standaloneScene(4096));
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.generation().entries.size(), 4096);
}

QTEST_GUILESS_MAIN(TaskListFactsProducerTests)
#include "tst_task_list_facts_producer.moc"
