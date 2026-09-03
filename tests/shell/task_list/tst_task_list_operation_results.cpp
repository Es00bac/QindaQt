// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"
#include "qindaqt/shell/task_list/operations/task_list_operation_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/producer/task_list_producer_transport.h"

#include <QSignalSpy>
#include <QtTest>

#include "task_list_producer_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

namespace {

// Scriptable in-process transports: no bus, no timers of their own. The test
// drives replies and failures explicitly so exactly-once lineage is provable
// from the recorded call list.
class ResultProducerTransport final : public TaskListProducerTransport {
  Q_OBJECT

public:
  bool start(QString *error = nullptr) override {
    if (error) {
      error->clear();
    }
    return true;
  }
  void stop() override {}
  void requestRefresh(quint64 token, const QString &uniqueOwner) override {
    lastToken = token;
    lastOwner = uniqueOwner;
  }

  void publishBridgeScene(const QString &owner) {
    // A control-bridge container admits Submit/ReleaseContainer; the shared
    // standard scene is hybrid-authority and would pre-reject those paths.
    const QByteArray windows = windowsPayload(
        {windowJson(QStringLiteral("w1"), QStringLiteral("app.one")),
         windowJson(QStringLiteral("w2"), QStringLiteral("app.two"),
                    QStringLiteral("c1")),
         windowJson(QStringLiteral("w3"), QStringLiteral("app.three"),
                    QStringLiteral("c1"), false, true, true)});
    const QByteArray containers = containersPayload(
        {{QStringLiteral("c1"), 7, QStringLiteral("control-bridge")}});
    const QByteArray scope = scopePayload(
        {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                        {QStringLiteral("ws-1")}),
         scopeEntryJson(QStringLiteral("w2"), QStringLiteral("output-1"),
                        {QStringLiteral("ws-1")}),
         scopeEntryJson(QStringLiteral("w3"), QStringLiteral("output-1"),
                        {QStringLiteral("ws-1")})});
    Q_EMIT windowsRead(lastToken, owner, windows);
    Q_EMIT containersRead(lastToken, owner, containers);
    Q_EMIT scopeRead(lastToken, owner, scope);
  }

  quint64 lastToken = 0;
  QString lastOwner;
};

class ResultOperationTransport final : public TaskListOperationTransport {
  Q_OBJECT

public:
  bool submitTransaction(quint64 token, const QString &uniqueOwner,
                         const QByteArray &requestJson) override {
    calls.append({QStringLiteral("Submit"), token, uniqueOwner, requestJson,
                  {}});
    return sendSucceeds;
  }
  bool releaseContainer(quint64 token, const QString &uniqueOwner,
                        const QString &containerId) override {
    calls.append({QStringLiteral("ReleaseContainer"), token, uniqueOwner, {},
                  {containerId}});
    return sendSucceeds;
  }
  bool dockWindows(quint64 token, const QString &uniqueOwner,
                   const QString &targetWindowId,
                   const QString &incomingWindowId, const QString &orientation,
                   const QString &position, double ratio) override {
    calls.append({QStringLiteral("DockWindows"), token, uniqueOwner, {},
                  {targetWindowId, incomingWindowId, orientation, position,
                   QString::number(ratio)}});
    return sendSucceeds;
  }

  void emitReply(quint64 token, const QString &owner,
                 const QByteArray &payload) {
    Q_EMIT operationReplied(token, owner, payload);
  }
  void emitFailure(quint64 token, const QString &owner,
                   const QString &message) {
    Q_EMIT operationFailed(token, owner, message);
  }

  struct Call {
    QString method;
    quint64 token = 0;
    QString owner;
    QByteArray payload;
    QStringList arguments;
  };
  QVector<Call> calls;
  bool sendSucceeds = true;
};

TaskListFactsProducerTiming fastTiming() {
  TaskListFactsProducerTiming timing;
  timing.debounceMilliseconds = 1;
  timing.requestTimeoutMilliseconds = 80;
  timing.retryMilliseconds = {5, 10, 20};
  return timing;
}

struct ReadyFixture {
  TaskListSource source;
  ResultProducerTransport producerTransport;
  ResultOperationTransport operationTransport;
  TaskListFactsProducer producer;
  TaskListOperationAdapter adapter;
  QSignalSpy finishedSpy;

  ReadyFixture()
      : producer(producerTransport, source, fastTiming()),
        adapter(producer, operationTransport, 60),
        finishedSpy(&adapter, &TaskListOperationAdapter::operationFinished) {
    if (!producer.start()) {
      qFatal("fixture producer did not start");
    }
    Q_EMIT producerTransport.serviceOwnerChanged(QStringLiteral(":1.1"));
  }

  void makeReady() {
    QTRY_VERIFY_WITH_TIMEOUT(producerTransport.lastToken != 0, 2'000);
    producerTransport.publishBridgeScene(QStringLiteral(":1.1"));
    if (source.status() != TaskListSourceStatus::Ready) {
      qFatal("fixture scene did not reach Ready");
    }
  }

  quint64 revision() const { return source.revision(); }
  QString owner() const { return QStringLiteral(":1.1"); }
};

TaskListOperationResult firstResult(const QSignalSpy &spy) {
  return spy.constFirst().constFirst().value<TaskListOperationResult>();
}

} // namespace

// Reply mapping and exactly-once lineage: every admitted request publishes
// exactly one result and is never resubmitted, whatever the wire does next.
class TaskListOperationResultsTests final : public QObject {
  Q_OBJECT

private slots:
  void committedSubmitFinishesExactlyOnce();
  void conflictReplyCarriesTheCurrentRevision();
  void rejectedReplyCarriesTheWireCode();
  void malformedReplyIsUncertainAndNeverResubmitted();
  void busFailureAfterSendIsUncertain();
  void replyTimeoutIsUncertainAndIgnoresLateReplies();
  void ownerChangeInFlightIsUncertain();
  void staleReplyTokenIsIgnored();
  void releaseAndDockRepliesMapToCommitted();
};

void TaskListOperationResultsTests::committedSubmitFinishesExactlyOnce() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.activateContainerPage(
      QStringLiteral("c1"), QStringLiteral("page-2"), fixture.revision());
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
  const auto &call = fixture.operationTransport.calls.constFirst();
  QCOMPARE(call.method, QStringLiteral("Submit"));
  QCOMPARE(call.token, token);
  QCOMPARE(call.owner, fixture.owner());
  // The Submit request fences the exact container lineage: the wire
  // expectedRevision is the accepted generation's container revision.
  QVERIFY(call.payload.contains(
      QByteArrayLiteral("\"expectedRevision\":\"7\"")));
  QVERIFY(call.payload.contains(
      QByteArrayLiteral("\"type\":\"activate-page\"")));
  QVERIFY(fixture.adapter.operationInFlight());

  fixture.operationTransport.emitReply(
      token, fixture.owner(),
      QByteArrayLiteral("{\"status\":\"committed\",\"revision\":\"8\"}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.token, token);
  QCOMPARE(result.status, TaskListOperationStatus::Committed);
  QVERIFY(!fixture.adapter.operationInFlight());

  // Waiting past the reply timeout and a duplicate reply add nothing.
  QTest::qWait(120);
  fixture.operationTransport.emitReply(
      token, fixture.owner(), QByteArrayLiteral("{\"status\":\"committed\"}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationResultsTests::conflictReplyCarriesTheCurrentRevision() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.detachWindow(
      QStringLiteral("c1"), QStringLiteral("w3"), fixture.revision());
  fixture.operationTransport.emitReply(
      token, fixture.owner(),
      QByteArrayLiteral("{\"status\":\"conflict\",\"revision\":\"9\","
                        "\"failure\":{\"code\":\"revision-conflict\","
                        "\"message\":\"stale\"}}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.status, TaskListOperationStatus::Conflict);
  QCOMPARE(result.code, QStringLiteral("revision-conflict"));
  QCOMPARE(result.currentContainerRevision, quint64(9));
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationResultsTests::rejectedReplyCarriesTheWireCode() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.releaseContainer(QStringLiteral("c1"),
                                                         fixture.revision());
  QCOMPARE(fixture.operationTransport.calls.constFirst().method,
           QStringLiteral("ReleaseContainer"));
  fixture.operationTransport.emitReply(
      token, fixture.owner(),
      QByteArrayLiteral("{\"status\":\"rejected\",\"failure\":{"
                        "\"code\":\"unknown-container\","
                        "\"message\":\"gone\"}}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.status, TaskListOperationStatus::Rejected);
  QCOMPARE(result.code, QStringLiteral("unknown-container"));
  QCOMPARE(result.message, QStringLiteral("gone"));
}

void TaskListOperationResultsTests::malformedReplyIsUncertainAndNeverResubmitted() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.releaseContainer(QStringLiteral("c1"),
                                                         fixture.revision());
  // A reply the adapter cannot interpret may still have committed.
  fixture.operationTransport.emitReply(token, fixture.owner(),
                                       QByteArrayLiteral("{not json"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(firstResult(fixture.finishedSpy).status,
           TaskListOperationStatus::Uncertain);

  QTest::qWait(120);
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationResultsTests::busFailureAfterSendIsUncertain() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.releaseContainer(QStringLiteral("c1"),
                                                         fixture.revision());
  fixture.operationTransport.emitFailure(token, fixture.owner(),
                                         QStringLiteral("service unknown"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.status, TaskListOperationStatus::Uncertain);
  QCOMPARE(result.code, QStringLiteral("request-outcome-unknown"));
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationResultsTests::replyTimeoutIsUncertainAndIgnoresLateReplies() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.releaseContainer(QStringLiteral("c1"),
                                                         fixture.revision());
  QTRY_COMPARE_WITH_TIMEOUT(fixture.finishedSpy.size(), 1, 2'000);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.token, token);
  QCOMPARE(result.status, TaskListOperationStatus::Uncertain);
  QCOMPARE(result.code, QStringLiteral("reply-timeout"));

  // The timed-out request is never resubmitted and its late reply is fenced.
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
  fixture.operationTransport.emitReply(
      token, fixture.owner(), QByteArrayLiteral("{\"status\":\"released\"}"));
  QTest::qWait(30);
  QCOMPARE(fixture.finishedSpy.size(), 1);
}

void TaskListOperationResultsTests::ownerChangeInFlightIsUncertain() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.releaseContainer(QStringLiteral("c1"),
                                                         fixture.revision());
  QVERIFY(fixture.adapter.operationInFlight());
  // Owner loss while a request is in flight: the outcome is unknowable.
  Q_EMIT fixture.producerTransport.serviceOwnerChanged({});
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.token, token);
  QCOMPARE(result.status, TaskListOperationStatus::Uncertain);
  QCOMPARE(result.code, QStringLiteral("owner-changed-in-flight"));
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationResultsTests::staleReplyTokenIsIgnored() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.releaseContainer(QStringLiteral("c1"),
                                                         fixture.revision());
  fixture.operationTransport.emitReply(token + 1000, fixture.owner(),
                                       QByteArrayLiteral("{\"status\":"
                                                         "\"released\"}"));
  fixture.operationTransport.emitReply(
      token, QStringLiteral(":9.9"),
      QByteArrayLiteral("{\"status\":\"released\"}"));
  QVERIFY(fixture.adapter.operationInFlight());
  QCOMPARE(fixture.finishedSpy.size(), 0);

  fixture.operationTransport.emitReply(
      token, fixture.owner(), QByteArrayLiteral("{\"status\":\"released\"}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(firstResult(fixture.finishedSpy).status,
           TaskListOperationStatus::Committed);
}

void TaskListOperationResultsTests::releaseAndDockRepliesMapToCommitted() {
  ReadyFixture fixture;
  fixture.makeReady();

  const quint64 release = fixture.adapter.releaseContainer(
      QStringLiteral("c1"), fixture.revision());
  fixture.operationTransport.emitReply(
      release, fixture.owner(), QByteArrayLiteral("{\"status\":\"released\"}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(firstResult(fixture.finishedSpy).status,
           TaskListOperationStatus::Committed);

  const quint64 dock = fixture.adapter.dockWindows(
      QStringLiteral("w1"), QStringLiteral("w9"), QStringLiteral("horizontal"),
      QStringLiteral("second"), 0.5, fixture.revision());
  QCOMPARE(fixture.operationTransport.calls.at(1).method,
           QStringLiteral("DockWindows"));
  fixture.operationTransport.emitReply(
      dock, fixture.owner(), QByteArrayLiteral("{\"status\":\"docked\","
                                               "\"revision\":\"1\"}"));
  QCOMPARE(fixture.finishedSpy.size(), 2);
  QCOMPARE(fixture.finishedSpy.at(1)
               .constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::Committed);
}

QTEST_GUILESS_MAIN(TaskListOperationResultsTests)
#include "tst_task_list_operation_results.moc"
