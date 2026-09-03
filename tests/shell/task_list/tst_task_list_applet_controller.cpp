// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QSignalSpy>
#include <QtTest>

#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskListApplet;
using TaskListOperationTest::FakeOperationAuthority;
using TaskListAppletTest::FakeTaskListOperationPort;

namespace {

// Real T0 source + real T1 authority fake + recording port. publishReady
// mirrors the producer's publication boundary: a fresh accepted generation,
// the authority's revision/owner moved in step, then one stateChanged.
struct ControllerFixture {
  TaskListSource source;
  FakeOperationAuthority authority;
  FakeTaskListOperationPort port;
};

TaskListAppletGrants allGrants() { return {true, true, true}; }

QVector<TaskWindowFact> standardFacts() {
  return {TaskListTest::standalone(QStringLiteral("w1"),
                                   QStringLiteral("app.one")),
          TaskListTest::primary(QStringLiteral("w2"),
                                QStringLiteral("app.two"),
                                QStringLiteral("c1")),
          TaskListTest::member(QStringLiteral("w3"), QStringLiteral("c1"))};
}

quint64 publishReady(ControllerFixture &fixture,
                     const QVector<TaskWindowFact> &facts) {
  const auto evaluation = fixture.source.publishGeneration(facts);
  if (!evaluation.ok()) {
    return 0;
  }
  fixture.authority.revision = fixture.source.revision();
  fixture.authority.sourceStatus = TaskListSourceStatus::Ready;
  fixture.authority.owner = QStringLiteral(":1.1");
  Q_EMIT fixture.authority.stateChanged();
  return fixture.source.revision();
}

} // namespace

class TaskListAppletControllerTests final : public QObject {
  Q_OBJECT

private slots:
  void coldStartTransitionsThroughBoundedPhases();
  void readDenialWithholdsObservationAndDispatch();
  void activateGrantRefusesActivationOnly();
  void manageGrantRefusesManagementOnly();
  void staleRevisionAndUnknownTaskNeverDispatch();
  void acceptedIntentDispatchesExactlyOnce();
  void duplicateIntentOnPendingTaskIsRefused();
  void committedCompletionClearsPendingWithoutFeedback();
  void failureCompletionSetsTruthfulFeedback();
  void synchronousPortCompletionLeavesNoStaleMarker();
  void unknownTokenResultsAreDropped();
  void ownerLossKeepsPendingUntilTerminalResult();
  void containerOperationsFenceMembershipAndRevision();
  void dockWindowsResolvesPrimariesAndFences();
  void scopeSettersReprojectAndClearFeedbackDismisses();
};

void TaskListAppletControllerTests::coldStartTransitionsThroughBoundedPhases() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  QSignalSpy reprojectedSpy(&controller,
                            &TaskListAppletController::stateReprojected);
  QCOMPARE(controller.phaseText(), QStringLiteral("loading"));
  QCOMPARE(controller.phaseReasonText(),
           QStringLiteral("compositor-task-list-loading"));
  QCOMPARE(controller.entryCount(), 0);
  QCOMPARE(controller.canActivate(), false);
  QCOMPARE(controller.canManage(), false);

  // AGENT-NOTE (negative control): producer unavailability reported without
  // any accepted generation must surface degraded, never park on loading —
  // this is the repaired T1 behavior the whole lane exists to prove.
  fixture.source.markDegraded();
  fixture.authority.setUnavailable();
  QCOMPARE(controller.phaseText(), QStringLiteral("degraded"));
  QCOMPARE(controller.phaseReasonText(),
           QStringLiteral("compositor-task-list-unavailable"));
  QCOMPARE(controller.entryCount(), 0);

  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(revision, quint64(1));
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
  QCOMPARE(controller.phaseReasonText().isEmpty(), true);
  QCOMPARE(controller.entryCount(), 2);
  QCOMPARE(controller.totalEntryCount(), 2);
  QCOMPARE(controller.overflowCount(), 0);
  QCOMPARE(controller.canActivate(), true);
  QCOMPARE(controller.canManage(), true);
  QVERIFY(reprojectedSpy.size() >= 2);
  QCOMPARE(controller.projection().rows.at(0).taskId, QStringLiteral("w1"));
  QCOMPARE(controller.projection().rows.at(1).taskId, QStringLiteral("c1"));
}

void TaskListAppletControllerTests::readDenialWithholdsObservationAndDispatch() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, {false, true, true});
  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(controller.phaseText(), QStringLiteral("unavailable"));
  QCOMPARE(controller.phaseReasonText(),
           QStringLiteral("windows-read-not-granted"));
  QCOMPARE(controller.entryCount(), 0);
  QCOMPARE(controller.totalEntryCount(), 0);
  QCOMPARE(controller.windowsReadGranted(), false);
  QCOMPARE(controller.canActivate(), false);
  QCOMPARE(controller.canManage(), false);

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), false);
  QVERIFY(controller.feedback().contains(QStringLiteral("windows.read")));
  QCOMPARE(controller.minimizeTask(QStringLiteral("w1"), revision), false);
  QCOMPARE(controller.closeTask(QStringLiteral("w1"), revision), false);
  QCOMPARE(controller.ungroupContainer(QStringLiteral("c1"), revision), false);
  QCOMPARE(controller.activateContainerPage(QStringLiteral("c1"),
                                            QStringLiteral("w3"), revision),
           false);
  QCOMPARE(controller.dockWindows(QStringLiteral("w1"), QStringLiteral("c1"),
                                  QStringLiteral("horizontal"),
                                  QStringLiteral("second"), 0.5, revision),
           false);
  QCOMPARE(controller.feedbackPresent(), true);
  QCOMPARE(fixture.port.calls.size(), 0);
}

void TaskListAppletControllerTests::activateGrantRefusesActivationOnly() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, {true, false, true});
  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(controller.windowsActivateGranted(), false);
  QCOMPARE(controller.canActivate(), false);
  QCOMPARE(controller.canManage(), true);

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), false);
  QVERIFY(controller.feedback().contains(QStringLiteral("windows.activate")));
  QCOMPARE(controller.feedbackStatus(), QStringLiteral("warning"));
  QCOMPARE(fixture.port.calls.size(), 0);

  QCOMPARE(controller.minimizeTask(QStringLiteral("w1"), revision), true);
  QCOMPARE(fixture.port.calls.size(), 1);
  QCOMPARE(fixture.port.lastCall().request.kind, TaskIntentKind::Minimize);
}

void TaskListAppletControllerTests::manageGrantRefusesManagementOnly() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, {true, true, false});
  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(controller.windowsManageGranted(), false);
  QCOMPARE(controller.canActivate(), true);
  QCOMPARE(controller.canManage(), false);

  QCOMPARE(controller.minimizeTask(QStringLiteral("w1"), revision), false);
  QVERIFY(controller.feedback().contains(QStringLiteral("windows.manage")));
  QCOMPARE(controller.closeTask(QStringLiteral("w1"), revision), false);
  QCOMPARE(controller.ungroupContainer(QStringLiteral("c1"), revision), false);
  QCOMPARE(controller.activateContainerPage(QStringLiteral("c1"),
                                            QStringLiteral("w3"), revision),
           false);
  QCOMPARE(controller.detachContainerWindow(QStringLiteral("c1"),
                                            QStringLiteral("w3"), revision),
           false);
  QCOMPARE(controller.dockWindows(QStringLiteral("w1"), QStringLiteral("c1"),
                                  QStringLiteral("horizontal"),
                                  QStringLiteral("second"), 0.5, revision),
           false);
  QCOMPARE(fixture.port.calls.size(), 0);

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);
  QCOMPARE(fixture.port.calls.size(), 1);
}

void TaskListAppletControllerTests::staleRevisionAndUnknownTaskNeverDispatch() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision + 9), false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("the task list changed before the action")));
  QCOMPARE(controller.activateTask(QStringLiteral("w-ghost"), revision),
           false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("the window is no longer listed")));
  QCOMPARE(fixture.port.calls.size(), 0);

  // A fresh publication retires the displayed revision: intents against the
  // old one are refused before any dispatch.
  const quint64 newer = publishReady(fixture, standardFacts());
  QCOMPARE(newer, revision + 1);
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), false);
  QCOMPARE(fixture.port.calls.size(), 0);
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), newer), true);
  QCOMPARE(fixture.port.calls.size(), 1);
  QCOMPARE(fixture.port.lastCall().revision, newer);
}

void TaskListAppletControllerTests::acceptedIntentDispatchesExactlyOnce() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());

  QCOMPARE(controller.closeTask(QStringLiteral("c1"), revision), true);
  QCOMPARE(fixture.port.calls.size(), 1);
  const auto &call = fixture.port.lastCall();
  QCOMPARE(call.method, QStringLiteral("executeTaskIntent"));
  QCOMPARE(call.request.taskId, QStringLiteral("c1"));
  QCOMPARE(call.request.kind, TaskIntentKind::Close);
  QCOMPARE(call.request.expectedRevision, revision);
  // The port receives the source-arbitrated outcome, not test-side guesses:
  // container close resolves the primary plus every member window.
  QCOMPARE(call.outcome.entryKind, TaskEntryKind::Container);
  QCOMPARE(call.outcome.primaryWindowId, QStringLiteral("w2"));
  QCOMPARE(call.outcome.memberWindowIds,
           (QStringList{QStringLiteral("w2"), QStringLiteral("w3")}));
}

void TaskListAppletControllerTests::duplicateIntentOnPendingTaskIsRefused() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);
  QCOMPARE(controller.pendingOperationCount(), 1);
  QVERIFY(controller.entryRows()
              .constFirst()
              .toMap()
              .value(QStringLiteral("pending"))
              .toBool());

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("an operation is already pending")));
  QCOMPARE(controller.minimizeTask(QStringLiteral("w1"), revision), false);
  QCOMPARE(fixture.port.calls.size(), 1);

  // Another task stays dispatchable while the first is in flight.
  QCOMPARE(controller.activateTask(QStringLiteral("c1"), revision), true);
  QCOMPARE(controller.pendingOperationCount(), 2);
}

void TaskListAppletControllerTests::committedCompletionClearsPendingWithoutFeedback() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);

  fixture.port.complete(fixture.port.calls.constFirst(),
                        TaskListOperationStatus::Committed, {}, {});
  QCOMPARE(controller.pendingOperationCount(), 0);
  QCOMPARE(controller.feedbackPresent(), false);
  QVERIFY(!controller.entryRows()
               .constFirst()
               .toMap()
               .value(QStringLiteral("pending"))
               .toBool());
}

void TaskListAppletControllerTests::failureCompletionSetsTruthfulFeedback() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  QSignalSpy feedbackSpy(&controller,
                         &TaskListAppletController::feedbackChanged);
  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);

  fixture.port.complete(fixture.port.calls.constFirst(),
                        TaskListOperationStatus::Rejected,
                        QStringLiteral("compositor-busy"),
                        QStringLiteral("the compositor refused"));
  QCOMPARE(controller.pendingOperationCount(), 0);
  QCOMPARE(controller.feedback(),
           QStringLiteral("Activate: the compositor refused"));
  QCOMPARE(controller.feedbackStatus(), QStringLiteral("error"));
  QVERIFY(feedbackSpy.size() >= 1);

  QCOMPARE(controller.minimizeTask(QStringLiteral("w1"), revision), true);
  fixture.port.complete(fixture.port.calls.constLast(),
                        TaskListOperationStatus::Unavailable,
                        QStringLiteral("compositor-window-minimize-unavailable"),
                        QStringLiteral("window minimize is not on the wire"));
  QCOMPARE(controller.feedback(),
           QStringLiteral("Minimize: window minimize is not on the wire"));
  QCOMPARE(controller.feedbackStatus(), QStringLiteral("info"));
}

void TaskListAppletControllerTests::synchronousPortCompletionLeavesNoStaleMarker() {
  ControllerFixture fixture;
  // AGENT-GUARD (hostile seam): the port emits operationFinished INSIDE the
  // dispatch call, before the token is knowable; the controller must buffer,
  // attribute by token after the pending record exists, and leave no stale
  // pending marker behind.
  fixture.port.completion = FakeTaskListOperationPort::Completion::Synchronous;
  fixture.port.synchronousStatus = TaskListOperationStatus::Unavailable;
  fixture.port.synchronousCode =
      QStringLiteral("compositor-window-activate-unavailable");
  fixture.port.synchronousMessage =
      QStringLiteral("window activation is not on the wire");
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);
  QCOMPARE(fixture.port.calls.size(), 1);
  QCOMPARE(controller.pendingOperationCount(), 0);
  QCOMPARE(controller.feedback(),
           QStringLiteral(
               "Activate: window activation is not on the wire"));
  QCOMPARE(controller.feedbackStatus(), QStringLiteral("info"));
  QVERIFY(!controller.entryRows()
               .constFirst()
               .toMap()
               .value(QStringLiteral("pending"))
               .toBool());

  // The task is dispatchable again immediately: no orphaned marker.
  fixture.port.completion = FakeTaskListOperationPort::Completion::Manual;
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);
  QCOMPARE(controller.pendingOperationCount(), 1);
}

void TaskListAppletControllerTests::unknownTokenResultsAreDropped() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);
  QCOMPARE(controller.pendingOperationCount(), 1);

  TaskListOperationResult forged;
  forged.token = 4242;
  forged.status = TaskListOperationStatus::Committed;
  fixture.port.injectResult(forged);
  QCOMPARE(controller.pendingOperationCount(), 1);
  QCOMPARE(controller.feedbackPresent(), false);

  forged.token = fixture.port.calls.constFirst().token + 1000;
  forged.status = TaskListOperationStatus::Rejected;
  forged.message = QStringLiteral("forged failure");
  fixture.port.injectResult(forged);
  QCOMPARE(controller.pendingOperationCount(), 1);
  QCOMPARE(controller.feedbackPresent(), false);

  // A replayed result after the real completion is dropped too.
  const auto call = fixture.port.calls.constFirst();
  fixture.port.complete(call, TaskListOperationStatus::Committed, {}, {});
  QCOMPARE(controller.pendingOperationCount(), 0);
  fixture.port.complete(call, TaskListOperationStatus::Rejected,
                        QStringLiteral("replayed"),
                        QStringLiteral("replayed failure"));
  QCOMPARE(controller.feedbackPresent(), false);
}

void TaskListAppletControllerTests::ownerLossKeepsPendingUntilTerminalResult() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());
  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), true);

  fixture.source.markDegraded();
  fixture.authority.setUnavailable();
  QCOMPARE(controller.phaseText(), QStringLiteral("degraded"));
  QCOMPARE(controller.phaseReasonText(),
           QStringLiteral("compositor-task-list-unavailable"));
  // The retained generation stays visible and the in-flight marker survives:
  // the port owes exactly one terminal result per admitted request.
  QCOMPARE(controller.entryCount(), 2);
  QCOMPARE(controller.pendingOperationCount(), 1);

  QCOMPARE(controller.closeTask(QStringLiteral("c1"), revision), false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("the task-list source is unavailable")));
  QCOMPARE(fixture.port.calls.size(), 1);

  fixture.port.complete(fixture.port.calls.constFirst(),
                        TaskListOperationStatus::Uncertain,
                        QStringLiteral("owner-changed-in-flight"),
                        QStringLiteral("the compositor owner vanished"));
  QCOMPARE(controller.pendingOperationCount(), 0);
  QCOMPARE(controller.feedback(),
           QStringLiteral("Activate: the compositor owner vanished"));
  QCOMPARE(controller.feedbackStatus(), QStringLiteral("warning"));
}

void TaskListAppletControllerTests::containerOperationsFenceMembershipAndRevision() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());

  QCOMPARE(controller.activateContainerPage(
               QStringLiteral("c1"), QStringLiteral("w-foreign"), revision),
           false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("not a member of this container")));
  QCOMPARE(controller.activateContainerPage(
               QStringLiteral("w1"), QStringLiteral("w1"), revision),
           false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("the container is no longer listed")));
  QCOMPARE(controller.ungroupContainer(QStringLiteral("c1"), revision + 4),
           false);
  QCOMPARE(fixture.port.calls.size(), 0);

  QCOMPARE(controller.activateContainerPage(
               QStringLiteral("c1"), QStringLiteral("w3"), revision),
           true);
  QCOMPARE(fixture.port.calls.size(), 1);
  QCOMPARE(fixture.port.lastCall().method,
           QStringLiteral("activateContainerPage"));
  QCOMPARE(fixture.port.lastCall().firstId, QStringLiteral("c1"));
  QCOMPARE(fixture.port.lastCall().secondId, QStringLiteral("w3"));
  QCOMPARE(fixture.port.lastCall().revision, revision);
  fixture.port.complete(fixture.port.lastCall(),
                        TaskListOperationStatus::Committed, {}, {});

  // The primary is itself a member, so detaching the visible page is admitted.
  QCOMPARE(controller.detachContainerWindow(
               QStringLiteral("c1"), QStringLiteral("w2"), revision),
           true);
  QCOMPARE(fixture.port.lastCall().method, QStringLiteral("detachWindow"));
  QCOMPARE(fixture.port.lastCall().secondId, QStringLiteral("w2"));
  fixture.port.complete(fixture.port.lastCall(),
                        TaskListOperationStatus::Committed, {}, {});

  // Ungroup is the release arm of the shell-owned container close policy.
  QCOMPARE(controller.ungroupContainer(QStringLiteral("c1"), revision), true);
  QCOMPARE(fixture.port.lastCall().method,
           QStringLiteral("releaseContainer"));
  QCOMPARE(fixture.port.lastCall().firstId, QStringLiteral("c1"));
  QCOMPARE(fixture.port.lastCall().revision, revision);
  QCOMPARE(controller.pendingOperationCount(), 1);
}

void TaskListAppletControllerTests::dockWindowsResolvesPrimariesAndFences() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  const quint64 revision = publishReady(fixture, standardFacts());

  QCOMPARE(controller.dockWindows(QStringLiteral("w1"), QStringLiteral("c1"),
                                  QStringLiteral("horizontal"),
                                  QStringLiteral("second"), 0.5, revision + 2),
           false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("the task list changed before the action")));
  QCOMPARE(controller.dockWindows(QStringLiteral("w1"),
                                  QStringLiteral("w-ghost"),
                                  QStringLiteral("horizontal"),
                                  QStringLiteral("second"), 0.5, revision),
           false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("the window is no longer listed")));
  QCOMPARE(fixture.port.calls.size(), 0);

  QCOMPARE(controller.dockWindows(QStringLiteral("w1"), QStringLiteral("c1"),
                                  QStringLiteral("horizontal"),
                                  QStringLiteral("second"), 0.5, revision),
           true);
  QCOMPARE(fixture.port.calls.size(), 1);
  const auto &call = fixture.port.lastCall();
  QCOMPARE(call.method, QStringLiteral("dockWindows"));
  // Both sides resolve to the generation's primary window ids, never a
  // collapsed container identity or a suppressed member.
  QCOMPARE(call.firstId, QStringLiteral("w1"));
  QCOMPARE(call.secondId, QStringLiteral("w2"));
  QCOMPARE(call.orientation, QStringLiteral("horizontal"));
  QCOMPARE(call.position, QStringLiteral("second"));
  QCOMPARE(call.ratio, 0.5);
  QCOMPARE(call.revision, revision);

  QCOMPARE(controller.dockWindows(QStringLiteral("w1"), QStringLiteral("c1"),
                                  QStringLiteral("vertical"),
                                  QStringLiteral("first"), 0.5, revision),
           false);
  QVERIFY(controller.feedback().contains(
      QStringLiteral("an operation is already pending")));
  QCOMPARE(fixture.port.calls.size(), 1);
}

void TaskListAppletControllerTests::scopeSettersReprojectAndClearFeedbackDismisses() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  publishReady(fixture, standardFacts());
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

  controller.setScopeOutputId(QStringLiteral("output-absent"));
  QCOMPARE(controller.scopeOutputId(), QStringLiteral("output-absent"));
  QCOMPARE(controller.phaseText(), QStringLiteral("empty"));
  QCOMPARE(controller.phaseReasonText(),
           QStringLiteral("no-windows-in-scope"));
  QCOMPARE(controller.entryCount(), 0);
  controller.setScopeOutputId({});
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
  QCOMPARE(controller.entryCount(), 2);

  controller.setScopeWorkspaceId(QStringLiteral("ws-absent"));
  QCOMPARE(controller.phaseText(), QStringLiteral("empty"));
  controller.setScopeWorkspaceId({});
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));

  QCOMPARE(controller.feedbackPresent(), false);
  controller.clearFeedback();
  QCOMPARE(controller.feedbackPresent(), false);

  controller.activateTask(QStringLiteral("w1"), quint64(999));
  QCOMPARE(controller.feedbackPresent(), true);
  controller.clearFeedback();
  QCOMPARE(controller.feedbackPresent(), false);
  QCOMPARE(controller.feedback().isEmpty(), true);
}

QTEST_GUILESS_MAIN(TaskListAppletControllerTests)
#include "tst_task_list_applet_controller.moc"
