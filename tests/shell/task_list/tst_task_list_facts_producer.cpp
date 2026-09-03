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

  // The bounded retry then admits the coherent standard scene.
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  transport.emitScene(transport.requestedTokens.at(1), QStringLiteral(":1.1"),
                      standardScene());
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

QTEST_GUILESS_MAIN(TaskListFactsProducerTests)
#include "tst_task_list_facts_producer.moc"
