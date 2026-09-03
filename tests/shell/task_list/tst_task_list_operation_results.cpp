// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"

#include <QtTest>

#include "task_list_operation_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace TaskListOperationTest;

namespace {

QByteArray dockedReply(const QString &suffix) {
  return QJsonDocument(
             QJsonObject{{QStringLiteral("protocol"),
                          QJsonObject{{QStringLiteral("major"), 1},
                                      {QStringLiteral("minor"), 1}}},
                         {QStringLiteral("transactionId"),
                          QStringLiteral("dock-%1").arg(suffix)},
                         {QStringLiteral("containerId"),
                          QStringLiteral("container-%1").arg(suffix)},
                         {QStringLiteral("status"),
                          QStringLiteral("docked")},
                         {QStringLiteral("revision"), QStringLiteral("1")}})
      .toJson(QJsonDocument::Compact);
}

} // namespace

// Reply mapping and exactly-once lineage: every admitted request publishes
// exactly one result and is never resubmitted, whatever the wire does next.
// Canonical-lineage hostile cases (forged, recycled, cross-lifetime replies)
// live in tst_task_list_operation_lineage.cpp.
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
  ReadyBridgeFixture fixture;
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
      submitReply(call.payload, QStringLiteral("committed"),
                  QStringLiteral("8")));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.token, token);
  QCOMPARE(result.status, TaskListOperationStatus::Committed);
  QVERIFY(!fixture.adapter.operationInFlight());

  // Waiting past the reply timeout and a duplicate reply add nothing.
  QTest::qWait(120);
  fixture.operationTransport.emitReply(
      token, fixture.owner(),
      submitReply(call.payload, QStringLiteral("committed"),
                  QStringLiteral("8")));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationResultsTests::conflictReplyCarriesTheCurrentRevision() {
  ReadyBridgeFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.detachWindow(
      QStringLiteral("c1"), QStringLiteral("w3"), fixture.revision());
  QByteArray reply = submitReply(
      fixture.operationTransport.calls.constFirst().payload,
      QStringLiteral("conflict"), QStringLiteral("9"));
  // The conflict reply also carries the failure object on the wire.
  QJsonObject root = QJsonDocument::fromJson(reply).object();
  root.insert(QStringLiteral("failure"),
              QJsonObject{{QStringLiteral("code"),
                           QStringLiteral("revision-conflict")},
                          {QStringLiteral("message"),
                           QStringLiteral("stale")}});
  reply = QJsonDocument(root).toJson(QJsonDocument::Compact);
  fixture.operationTransport.emitReply(token, fixture.owner(), reply);
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.status, TaskListOperationStatus::Conflict);
  QCOMPARE(result.code, QStringLiteral("revision-conflict"));
  QCOMPARE(result.currentContainerRevision, quint64(9));
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationResultsTests::rejectedReplyCarriesTheWireCode() {
  ReadyBridgeFixture fixture;
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
  ReadyBridgeFixture fixture;
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
  ReadyBridgeFixture fixture;
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
  ReadyBridgeFixture fixture;
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
  ReadyBridgeFixture fixture;
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
  ReadyBridgeFixture fixture;
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
  ReadyBridgeFixture fixture;
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
  fixture.operationTransport.emitReply(dock, fixture.owner(),
                                       dockedReply(QStringLiteral("abc123")));
  QCOMPARE(fixture.finishedSpy.size(), 2);
  QCOMPARE(fixture.finishedSpy.at(1)
               .constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::Committed);
}

QTEST_GUILESS_MAIN(TaskListOperationResultsTests)
#include "tst_task_list_operation_results.moc"
