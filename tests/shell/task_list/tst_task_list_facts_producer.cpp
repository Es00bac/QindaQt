// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

#include <QSignalSpy>
#include <QtTest>

#include "task_list_producer_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

namespace {

class FakeProducerTransport final : public TaskListProducerTransport {
  Q_OBJECT

public:
  bool start(QString *error = nullptr) override {
    if (error) {
      error->clear();
    }
    started = startSucceeds;
    return startSucceeds;
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
  void emitFacts(quint64 token, const QString &owner,
                 const QByteArray &payload) {
    Q_EMIT factsRead(token, owner, payload);
  }
  void emitFailure(quint64 token, const QString &owner,
                   const QString &message) {
    Q_EMIT refreshFailed(token, owner, message);
  }

  bool started = false;
  bool startSucceeds = true;
  QVector<quint64> requestedTokens;
  QStringList requestedOwners;
};

TaskListFactsProducerTiming fastTiming() {
  TaskListFactsProducerTiming timing;
  timing.debounceMilliseconds = 1;
  timing.requestTimeoutMilliseconds = 40;
  timing.retryMilliseconds = {5, 10, 20};
  return timing;
}

void bindAndWait(FakeProducerTransport &transport,
                 const QString &owner = QStringLiteral(":1.1")) {
  transport.emitOwner(owner);
  QTRY_VERIFY_WITH_TIMEOUT(!transport.requestedTokens.isEmpty(), 2'000);
}

} // namespace

class TaskListFactsProducerTests final : public QObject {
  Q_OBJECT

private slots:
  void coherentFactsPublishReadyGeneration();
  void failedRefreshRetainsGenerationAndSignals();
  void invalidationRaceAndLateRepliesAreFenced();
  void malformedInventoryDegradesAndSignals();
  void nonReplyingAuthorityDegradesSignalsAndRetries();
  void retryScheduleStopsWithoutPolling();
  void ownerLossAndReplacementFenceOldReplies();
  void foreignEpochRegressionAndCollisionAreRejected();
  void stopWithdrawsAvailabilityAndSignals();
  void atLimitInventoryPublishesAtomically();
};

void TaskListFactsProducerTests::coherentFactsPublishReadyGeneration() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
  QVERIFY(producer.start());
  bindAndWait(transport);

  const quint64 token = transport.requestedTokens.constFirst();
  transport.emitFacts(token, QStringLiteral(":1.1"),
                      standardScene().taskFacts);
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(1));
  QCOMPARE(source.generation().entries.size(), 2);
  QCOMPARE(producer.actionGeneration(),
           std::optional(TaskListActionGeneration{kWindowEpoch, 11}));
  const auto lineage = producer.containerLineage(QStringLiteral("c1"));
  QVERIFY(lineage.has_value());
  QCOMPARE(lineage->revision, quint64(7));
  QCOMPARE(producer.lastError(), QString());
  QVERIFY(stateSpy.size() >= 2);
}

// AGENT-NOTE: Review finding P1-2 on rejected candidate 3a5ae17: failRefresh()
// changed availability without notifying its sole observer. Every failure is
// now observable and retains the last generation unchanged.
void TaskListFactsProducerTests::failedRefreshRetainsGenerationAndSignals() {
  TaskListSource source;
  const TaskListEvaluation published = source.publishGeneration(
      {TaskListTest::standalone(QStringLiteral("w-old"),
                                QStringLiteral("app.old"))});
  QVERIFY(published.ok());
  const TaskGeneration retained = source.generation();

  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
  QVERIFY(producer.start());
  bindAndWait(transport);
  const qsizetype beforeFailure = stateSpy.size();
  transport.emitFailure(transport.requestedTokens.constFirst(),
                        QStringLiteral(":1.1"), QStringLiteral("broken"));
  QCOMPARE(stateSpy.size(), beforeFailure + 1);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(source.generation(), retained);
}

void TaskListFactsProducerTests::invalidationRaceAndLateRepliesAreFenced() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  bindAndWait(transport);
  const quint64 first = transport.requestedTokens.constFirst();

  transport.emitInvalidation(QStringLiteral(":1.1"));
  transport.emitFacts(first, QStringLiteral(":1.1"),
                      standardScene().taskFacts);
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 2, 2'000);
  QCOMPARE(source.revision(), quint64(0));

  transport.emitFacts(first, QStringLiteral(":1.1"),
                      standardScene().taskFacts);
  QVERIFY(producer.refreshInFlight());
  transport.emitFacts(transport.requestedTokens.at(1),
                      QStringLiteral(":1.1"), standardScene(2).taskFacts);
  QVERIFY(!producer.refreshInFlight());
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
}

void TaskListFactsProducerTests::malformedInventoryDegradesAndSignals() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
  QVERIFY(producer.start());
  bindAndWait(transport);
  const qsizetype before = stateSpy.size();
  transport.emitFacts(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), QByteArrayLiteral("{bad"));
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(stateSpy.size(), before + 1);
  QVERIFY(producer.lastError().contains(QStringLiteral("object")));
}

// AGENT-NOTE: Review finding P2-1 on rejected candidate 3a5ae17 required a
// registered non-replying-authority control. The timeout publishes Degraded
// and stateChanged before one bounded retry; it never blocks the event loop.
void TaskListFactsProducerTests::nonReplyingAuthorityDegradesSignalsAndRetries() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
  QVERIFY(producer.start());
  bindAndWait(transport);
  const qsizetype before = stateSpy.size();
  QTRY_VERIFY_WITH_TIMEOUT(transport.requestedTokens.size() >= 2, 2'000);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QVERIFY(stateSpy.size() > before);
  QVERIFY(producer.lastError().contains(QStringLiteral("timed out")));
}

void TaskListFactsProducerTests::retryScheduleStopsWithoutPolling() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  bindAndWait(transport);

  // One initial read plus each of the three configured recovery attempts.
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), 4, 2'000);
  QTest::qWait(100);
  QCOMPARE(transport.requestedTokens.size(), 4);
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
}

void TaskListFactsProducerTests::ownerLossAndReplacementFenceOldReplies() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  bindAndWait(transport, QStringLiteral(":1.1"));
  const quint64 oldToken = transport.requestedTokens.constFirst();

  transport.emitOwner({});
  QCOMPARE(producer.uniqueOwner(), QString());
  QVERIFY(!producer.actionGeneration());
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  transport.emitFacts(oldToken, QStringLiteral(":1.1"),
                      standardScene().taskFacts);
  QCOMPARE(source.revision(), quint64(0));

  const qsizetype requestsBeforeReplacement = transport.requestedTokens.size();
  transport.emitOwner(QStringLiteral(":1.2"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(),
                            requestsBeforeReplacement + 1, 2'000);
  QCOMPARE(producer.uniqueOwner(), QStringLiteral(":1.2"));
  transport.emitFacts(oldToken, QStringLiteral(":1.1"),
                      standardScene().taskFacts);
  QVERIFY(producer.refreshInFlight());
}

void TaskListFactsProducerTests::foreignEpochRegressionAndCollisionAreRejected() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
  QVERIFY(producer.start());
  bindAndWait(transport);
  transport.emitFacts(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"), standardScene(5).taskFacts);

  qsizetype previous = transport.requestedTokens.size();
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), previous + 1,
                            2'000);
  const qsizetype beforeRegression = stateSpy.size();
  transport.emitFacts(transport.requestedTokens.constLast(),
                      QStringLiteral(":1.1"),
                      standardScene(4).taskFacts);
  QCOMPARE(stateSpy.size(), beforeRegression + 1);
  QVERIFY(producer.lastError().contains(QStringLiteral("lineage")));

  previous = transport.requestedTokens.size();
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), previous + 1,
                            2'000);
  transport.emitFacts(
      transport.requestedTokens.constLast(), QStringLiteral(":1.1"),
      standardScene(6, QStringLiteral("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb"))
          .taskFacts);
  QVERIFY(producer.lastError().contains(QStringLiteral("lineage")));

  QByteArray collision = standardScene(5).taskFacts;
  collision.replace("Title w1", "Changed w1");
  previous = transport.requestedTokens.size();
  transport.emitInvalidation(QStringLiteral(":1.1"));
  QTRY_COMPARE_WITH_TIMEOUT(transport.requestedTokens.size(), previous + 1,
                            2'000);
  transport.emitFacts(transport.requestedTokens.constLast(),
                      QStringLiteral(":1.1"), collision);
  QVERIFY(producer.lastError().contains(QStringLiteral("lineage")));
  QCOMPARE(source.revision(), quint64(1));
}

// AGENT-NOTE: Review finding P1-2 on rejected candidate 3a5ae17: stop left a
// Ready producer owner live. Stop now clears the owner, marks Degraded even
// before a first generation, and emits the availability transition.
void TaskListFactsProducerTests::stopWithdrawsAvailabilityAndSignals() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
  QVERIFY(producer.start());
  bindAndWait(transport);
  const qsizetype beforeStop = stateSpy.size();
  producer.stop();
  QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
  QCOMPARE(producer.uniqueOwner(), QString());
  QVERIFY(!producer.refreshInFlight());
  QCOMPARE(stateSpy.size(), beforeStop + 1);
}

void TaskListFactsProducerTests::atLimitInventoryPublishesAtomically() {
  TaskListSource source;
  FakeProducerTransport transport;
  TaskListFactsProducer producer(transport, source, fastTiming());
  QVERIFY(producer.start());
  bindAndWait(transport);
  transport.emitFacts(transport.requestedTokens.constFirst(),
                      QStringLiteral(":1.1"),
                      standaloneScene(4096).taskFacts);
  QCOMPARE(source.status(), TaskListSourceStatus::Ready);
  QCOMPARE(source.revision(), quint64(1));
  QCOMPARE(source.generation().entries.size(), 4096);
}

QTEST_GUILESS_MAIN(TaskListFactsProducerTests)
#include "tst_task_list_facts_producer.moc"
