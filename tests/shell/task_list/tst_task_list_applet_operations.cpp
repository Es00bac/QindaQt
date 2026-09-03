// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_operation_bridge.h"

#include <QSignalSpy>
#include <QtTest>

#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskListApplet;
using namespace TaskListOperationTest;

namespace {

// Real T0 source with an accepted generation, the real T1 adapter over the
// recording transport, and the real applet bridge. The authority's revision
// is moved in step with the source so adapter fencing sees one publication
// boundary.
struct DispatchFixture {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeOperationTransport operationTransport;
  TaskListOperationAdapter adapter;
  TaskListAppletOperationBridge bridge;
  QSignalSpy finishedSpy;

  DispatchFixture()
      : adapter(authority, operationTransport, 100),
        bridge(adapter),
        finishedSpy(&bridge,
                    &TaskListAppletOperationPort::operationFinished) {}

  quint64 publishReady() {
    const auto evaluation = source.publishGeneration(
        {TaskListTest::standalone(QStringLiteral("w1"),
                                  QStringLiteral("app.one")),
         TaskListTest::primary(QStringLiteral("w2"),
                               QStringLiteral("app.two"),
                               QStringLiteral("c1")),
         TaskListTest::member(QStringLiteral("w3"), QStringLiteral("c1"))});
    if (!evaluation.ok()) {
      return 0;
    }
    authority.revision = source.revision();
    return source.revision();
  }

  [[nodiscard]] TaskListOperationResult firstResult() const {
    return finishedSpy.constFirst()
        .constFirst()
        .value<TaskListOperationResult>();
  }
};

} // namespace

class TaskListAppletOperationDispatchTests final : public QObject {
  Q_OBJECT

private slots:
  void windowActivateFinishesUnavailableWithoutBusTraffic();
  void releaseContainerDispatchesOnceAndCommits();
  void secondRequestWhileInFlightIsBusy();
  void revisionMismatchIsStaleBeforeBusTraffic();
  void degradedAuthorityIsSourceNotReady();
  void controllerIntentReachesTheAdapterSynchronously();
};

void TaskListAppletOperationDispatchTests::windowActivateFinishesUnavailableWithoutBusTraffic() {
  DispatchFixture fixture;
  const quint64 revision = fixture.publishReady();
  QVERIFY(revision > 0);

  TaskIntentRequest request;
  request.taskId = QStringLiteral("w1");
  request.kind = TaskIntentKind::Activate;
  request.expectedRevision = revision;
  const TaskIntentOutcome outcome = fixture.source.requestIntent(request);
  QVERIFY(outcome.ok());

  // The bridge forwards verbatim; window-level activate finishes Unavailable
  // in this adapter (ADR-0061) and may do so synchronously.
  fixture.bridge.executeTaskIntent(request, outcome);
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.firstResult().status, TaskListOperationStatus::Unavailable);
  QCOMPARE(fixture.firstResult().code,
           QStringLiteral("compositor-window-activate-unavailable"));
  QCOMPARE(fixture.operationTransport.calls.size(), 0);
}

void TaskListAppletOperationDispatchTests::releaseContainerDispatchesOnceAndCommits() {
  DispatchFixture fixture;
  const quint64 revision = fixture.publishReady();
  QVERIFY(revision > 0);

  const quint64 token =
      fixture.bridge.releaseContainer(QStringLiteral("c1"), revision);
  QVERIFY(token != 0);
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
  QCOMPARE(fixture.operationTransport.calls.constFirst().method,
           QStringLiteral("ReleaseContainer"));
  QCOMPARE(fixture.operationTransport.calls.constFirst().arguments,
           (QStringList{QStringLiteral("c1")}));
  QCOMPARE(fixture.finishedSpy.size(), 0);

  fixture.operationTransport.emitReply(
      token, fixture.authority.owner,
      QByteArrayLiteral("{\"status\":\"released\"}"));
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.firstResult().status, TaskListOperationStatus::Committed);
  QCOMPARE(fixture.firstResult().token, token);
}

void TaskListAppletOperationDispatchTests::secondRequestWhileInFlightIsBusy() {
  DispatchFixture fixture;
  const quint64 revision = fixture.publishReady();
  QVERIFY(revision > 0);

  fixture.bridge.releaseContainer(QStringLiteral("c1"), revision);
  QVERIFY(fixture.adapter.operationInFlight());
  fixture.bridge.releaseContainer(QStringLiteral("c1"), revision);
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.firstResult().status, TaskListOperationStatus::Busy);
  QCOMPARE(fixture.operationTransport.calls.size(), 1);
}

void TaskListAppletOperationDispatchTests::revisionMismatchIsStaleBeforeBusTraffic() {
  DispatchFixture fixture;
  const quint64 revision = fixture.publishReady();
  QVERIFY(revision > 0);

  fixture.bridge.releaseContainer(QStringLiteral("c1"), revision + 41);
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.firstResult().status,
           TaskListOperationStatus::StaleGeneration);
  QCOMPARE(fixture.operationTransport.calls.size(), 0);
}

void TaskListAppletOperationDispatchTests::degradedAuthorityIsSourceNotReady() {
  DispatchFixture fixture;
  const quint64 revision = fixture.publishReady();
  QVERIFY(revision > 0);
  fixture.authority.setUnavailable();

  fixture.bridge.releaseContainer(QStringLiteral("c1"), revision);
  QCOMPARE(fixture.finishedSpy.size(), 1);
  QCOMPARE(fixture.firstResult().status,
           TaskListOperationStatus::SourceNotReady);
  QCOMPARE(fixture.operationTransport.calls.size(), 0);
}

// Full-stack proof: the real controller drives the real bridge and adapter.
// The adapter's fenced Unavailable result arrives synchronously INSIDE the
// dispatch call — the hostile seam — so the controller must leave no pending
// marker and surface truthful feedback immediately.
void TaskListAppletOperationDispatchTests::controllerIntentReachesTheAdapterSynchronously() {
  DispatchFixture fixture;
  const quint64 revision = fixture.publishReady();
  QVERIFY(revision > 0);
  Q_EMIT fixture.authority.stateChanged();
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.bridge, {true, true, true});
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);
  QCOMPARE(fixture.operationTransport.calls.size(), 0);
  QCOMPARE(controller.pendingOperationCount(), 0);
  QCOMPARE(controller.feedbackPresent(), true);
  QVERIFY(controller.feedback().startsWith(QStringLiteral("Activate: ")));
  QCOMPARE(controller.feedbackStatus(), QStringLiteral("info"));
}

QTEST_GUILESS_MAIN(TaskListAppletOperationDispatchTests)
#include "tst_task_list_applet_operations.moc"
