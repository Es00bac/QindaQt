// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/task_list_operation_adapter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "task_list_operation_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace TaskListOperationTest;

// Canonical reply-lineage hostile controls (review finding P1-3 on rejected
// candidate 3a5ae17): a reply settles an in-flight request only when the full
// canonical lineage (protocol, transactionId, containerId, status, revision)
// echoes the submitted transaction, and tokens are bound to the adapter
// lifetime so destroyed-adapter replies can never recycle into a new request.
class TaskListOperationLineageTests final : public QObject {
  Q_OBJECT

private slots:
  void submitReplyWithoutCanonicalEchoIsUncertain();
  void forgedSubmitReplyLineageIsUncertain();
  void committedRevisionMustAdvanceFromTheExpectedRevision();
  void adapterReconstructionCannotRecycleLineage();
};

void TaskListOperationLineageTests::submitReplyWithoutCanonicalEchoIsUncertain() {
  ReadyBridgeFixture fixture;
  fixture.makeReady();

  const quint64 token = fixture.adapter.activateContainerPage(
      QStringLiteral("c1"), QStringLiteral("page-2"), fixture.revision());
  fixture.operationTransport.emitReply(
      token, fixture.owner(),
      QByteArrayLiteral("{\"status\":\"committed\",\"revision\":\"8\"}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  const TaskListOperationResult result = firstResult(fixture.finishedSpy);
  QCOMPARE(result.status, TaskListOperationStatus::Uncertain);
  QCOMPARE(result.code, QStringLiteral("reply-lineage-mismatch"));
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListOperationLineageTests::forgedSubmitReplyLineageIsUncertain() {
  ReadyBridgeFixture fixture;
  fixture.makeReady();

  const auto attempt = [&fixture](const QString &containerOverride,
                                  const QString &transactionOverride) {
    const quint64 token = fixture.adapter.activateContainerPage(
        QStringLiteral("c1"), QStringLiteral("page-2"), fixture.revision());
    fixture.operationTransport.emitReply(
        token, fixture.owner(),
        submitReply(fixture.operationTransport.calls.constLast().payload,
                    QStringLiteral("committed"), QStringLiteral("8"),
                    containerOverride, transactionOverride));
    return token;
  };

  // Foreign container echo.
  attempt(QStringLiteral("c-forged"), {});
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(firstResult(fixture.finishedSpy).status,
           TaskListOperationStatus::Uncertain);

  // Foreign transaction echo.
  attempt({}, QStringLiteral("tasklist-forged"));
  QCOMPARE(fixture.finishedSpy.size(), 2);
  QCOMPARE(fixture.finishedSpy.at(1)
               .constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::Uncertain);

  // Unsupported protocol echo.
  const quint64 token = fixture.adapter.activateContainerPage(
      QStringLiteral("c1"), QStringLiteral("page-2"), fixture.revision());
  QByteArray reply = submitReply(
      fixture.operationTransport.calls.constLast().payload,
      QStringLiteral("committed"), QStringLiteral("8"));
  QJsonObject root = QJsonDocument::fromJson(reply).object();
  root.insert(QStringLiteral("protocol"),
              QJsonObject{{QStringLiteral("major"), 2},
                          {QStringLiteral("minor"), 0}});
  fixture.operationTransport.emitReply(
      token, fixture.owner(),
      QJsonDocument(root).toJson(QJsonDocument::Compact));
  QCOMPARE(fixture.finishedSpy.size(), 3);
  QCOMPARE(fixture.finishedSpy.at(2)
               .constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::Uncertain);
}

void TaskListOperationLineageTests::committedRevisionMustAdvanceFromTheExpectedRevision() {
  ReadyBridgeFixture fixture;
  fixture.makeReady();

  // The accepted generation has container revision 7; a commit must report 8.
  const quint64 wrong = fixture.adapter.activateContainerPage(
      QStringLiteral("c1"), QStringLiteral("page-2"), fixture.revision());
  fixture.operationTransport.emitReply(
      wrong, fixture.owner(),
      submitReply(fixture.operationTransport.calls.constLast().payload,
                  QStringLiteral("committed"), QStringLiteral("9")));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(firstResult(fixture.finishedSpy).status,
           TaskListOperationStatus::Uncertain);

  const quint64 right = fixture.adapter.activateContainerPage(
      QStringLiteral("c1"), QStringLiteral("page-2"), fixture.revision());
  fixture.operationTransport.emitReply(
      right, fixture.owner(),
      submitReply(fixture.operationTransport.calls.constLast().payload,
                  QStringLiteral("committed"), QStringLiteral("8")));
  QCOMPARE(fixture.finishedSpy.size(), 2);
  QCOMPARE(fixture.finishedSpy.at(1)
               .constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::Committed);
}

void TaskListOperationLineageTests::adapterReconstructionCannotRecycleLineage() {
  ReadyBridgeFixture fixture;
  fixture.makeReady();

  quint64 oldToken = 0;
  {
    TaskListOperationAdapter adapterA(fixture.producer,
                                      fixture.operationTransport, 60);
    oldToken = adapterA.releaseContainer(QStringLiteral("c1"),
                                         fixture.revision());
    QVERIFY(adapterA.operationInFlight());
  }

  TaskListOperationAdapter adapterB(fixture.producer,
                                    fixture.operationTransport, 60);
  QSignalSpy spyB(&adapterB, &TaskListOperationAdapter::operationFinished);
  const quint64 newToken = adapterB.dockWindows(
      QStringLiteral("w1"), QStringLiteral("w9"), QStringLiteral("horizontal"),
      QStringLiteral("second"), 0.5, fixture.revision());
  QVERIFY(adapterB.operationInFlight());
  // Lineage is bound to the adapter lifetime: the token cannot recycle.
  QVERIFY(newToken != oldToken);

  // The late reply from the destroyed adapter is unknown lineage.
  fixture.operationTransport.emitReply(oldToken, fixture.owner(),
                                       QByteArrayLiteral("{\"status\":"
                                                         "\"released\"}"));
  QTest::qWait(30);
  QVERIFY(adapterB.operationInFlight());
  QCOMPARE(spyB.size(), 0);

  // The request still settles normally on its own reply.
  fixture.operationTransport.emitReply(
      newToken, fixture.owner(),
      QJsonDocument(
          QJsonObject{{QStringLiteral("protocol"),
                       QJsonObject{{QStringLiteral("major"), 1},
                                   {QStringLiteral("minor"), 1}}},
                      {QStringLiteral("transactionId"),
                       QStringLiteral("dock-def456")},
                      {QStringLiteral("containerId"),
                       QStringLiteral("container-def456")},
                      {QStringLiteral("status"), QStringLiteral("docked")},
                      {QStringLiteral("revision"), QStringLiteral("1")}})
          .toJson(QJsonDocument::Compact));
  QCOMPARE(spyB.size(), 1);
  QCOMPARE(spyB.constFirst()
               .constFirst()
               .value<TaskListOperationResult>()
               .status,
           TaskListOperationStatus::Committed);
}

QTEST_GUILESS_MAIN(TaskListOperationLineageTests)
#include "tst_task_list_operation_lineage.moc"
