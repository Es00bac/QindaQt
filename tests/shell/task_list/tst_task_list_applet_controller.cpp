// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QSignalSpy>
#include <QQmlEngine>
#include <QtTest>

#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskListApplet;
using TaskListOperationTest::FakeOperationAuthority;
using TaskListAppletTest::FakeTaskListOperationPort;
using TaskListAppletTest::FakePreviewPort;

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

QVector<TaskWindowFact> standardFacts() { return {
          TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.one")),
          TaskListTest::primary(QStringLiteral("w2"), QStringLiteral("app.two"),
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
  void presentationLimitRaisesDockBoundAndClamps();
  void userOrderOverlayReordersDisplayedRowsWithoutCommitting();
  void reorderTaskFencesStaleRevisionAndUnknownIds();
  void reorderTaskCommitsAndPersistsAcrossGenerations();
  void previewRequestsAreFencedBoundedAndSuperseded();
  void iconEvidenceRequiresBothMetadataAndThemeResolution();
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
  QCOMPARE(controller.totalWindowCount(), 3);
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
  QCOMPARE(controller.totalWindowCount(), 0);
  QCOMPARE(controller.windowsReadGranted(), false);
  QCOMPARE(controller.canActivate(), false);
  QCOMPARE(controller.canManage(), false);

  QCOMPARE(controller.activateTask(QStringLiteral("w1"), revision), false);
  QVERIFY(controller.feedback().contains(QStringLiteral("windows.read")));
  QCOMPARE(controller.minimizeTask(QStringLiteral("w1"), revision), false);
  QCOMPARE(controller.closeTask(QStringLiteral("w1"), revision), false);
  QCOMPARE(controller.raiseTask(QStringLiteral("w1"), revision), false);
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
  QCOMPARE(controller.raiseTask(QStringLiteral("w1"), revision), false);
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

void TaskListAppletControllerTests::iconEvidenceRequiresBothMetadataAndThemeResolution() {
  ControllerFixture fixture;
  TaskListAppletController controller(
      fixture.source, fixture.authority, fixture.port, allGrants(),
      [](const QString &applicationId) {
        return applicationId == QLatin1StringView("app.one")
            ? QStringLiteral("preferences-system")
            : QStringLiteral("missing-icon");
      },
      [](const QString &iconName) {
        return iconName == QLatin1StringView("preferences-system");
      });
  publishReady(fixture, standardFacts());

  const QVariantList rows = controller.entryRows();
  QCOMPARE(rows.size(), 2);
  QCOMPARE(rows[0].toMap().value(QStringLiteral("iconName")),
           QVariant(QStringLiteral("preferences-system")));
  QCOMPARE(rows[0].toMap().value(QStringLiteral("iconResolved")), QVariant(true));
  QCOMPARE(rows[1].toMap().value(QStringLiteral("iconResolved")), QVariant(false));
}

void TaskListAppletControllerTests::presentationLimitRaisesDockBoundAndClamps() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  QCOMPARE(controller.presentationLimit(), kMaxPresentedTaskEntries);

  // One window past the taskbar cap, so overflow truth is exact in both modes.
  QVector<TaskWindowFact> facts;
  facts.reserve(kMaxPresentedTaskEntries + 1);
  for (int index = 0; index <= kMaxPresentedTaskEntries; ++index) {
    facts.append(TaskListTest::standalone(
        QStringLiteral("w-%1").arg(index, 4, 10, QLatin1Char('0')),
        QStringLiteral("app.%1").arg(index, 4, 10, QLatin1Char('0'))));
  }
  publishReady(fixture, facts);
  QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
  QCOMPARE(controller.entryCount(), kMaxPresentedTaskEntries);
  QCOMPARE(controller.overflowCount(), 1);

  // Raising the bound (dock host) presents every row; overflow truth follows.
  QSignalSpy reprojectedSpy(&controller,
                            &TaskListAppletController::stateReprojected);
  controller.setPresentationLimit(kMaxPresentedDockEntries);
  QCOMPARE(controller.presentationLimit(), kMaxPresentedDockEntries);
  QCOMPARE(controller.entryCount(), kMaxPresentedTaskEntries + 1);
  QCOMPARE(controller.overflowCount(), 0);
  QCOMPARE(reprojectedSpy.size(), 1);

  // The bound is clamped into [1, kMaxPresentedDockEntries]; a lower bound
  // truncates again with exact overflow truth.
  controller.setPresentationLimit(0);
  QCOMPARE(controller.presentationLimit(), 1);
  QCOMPARE(controller.entryCount(), 1);
  QCOMPARE(controller.overflowCount(), kMaxPresentedTaskEntries);

  // Setting the same value is a no-op without a reprojection.
  QSignalSpy reprojectedAgain(&controller,
                              &TaskListAppletController::stateReprojected);
  controller.setPresentationLimit(1);
  QCOMPARE(reprojectedAgain.size(), 0);
}

void TaskListAppletControllerTests::userOrderOverlayReordersDisplayedRowsWithoutCommitting() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  publishReady(fixture,
               {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1")),
                TaskListTest::standalone(QStringLiteral("w2"), QStringLiteral("app.2")),
                TaskListTest::standalone(QStringLiteral("w3"), QStringLiteral("app.3"))});
  QCOMPARE(controller.entryRows().at(0).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w1")));

  // Settings-fed overlay: the displayed (and traversal) order changes, but
  // no commit is emitted — settings is the source, not the user gesture.
  // Normalization drops the blanks and the duplicate before retaining.
  QSignalSpy committedSpy(
      &controller, &TaskListAppletController::taskOrderCommitted);
  controller.setUserTaskOrder({QStringLiteral("w3"), QString{},
                               QStringLiteral("w3"), QStringLiteral("   ")});
  const auto rows = controller.entryRows();
  QCOMPARE(rows.size(), 3);
  QCOMPARE(rows.at(0).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w3")));
  QCOMPARE(rows.at(0).toMap().value(QStringLiteral("keyboardIndex")),
           QVariant(1));
  QCOMPARE(rows.at(1).toMap().value(QStringLiteral("keyboardIndex")),
           QVariant(2));
  QCOMPARE(controller.userTaskOrder(),
           (QStringList{QStringLiteral("w3")}));
  QCOMPARE(committedSpy.size(), 0);

  // An exact echo (the settings snapshot observing its own write) is a
  // no-op: no reprojection and no property notification.
  QSignalSpy changedSpy(&controller,
                        &TaskListAppletController::userTaskOrderChanged);
  QSignalSpy reprojectedSpy(&controller,
                            &TaskListAppletController::stateReprojected);
  controller.setUserTaskOrder({QStringLiteral("w3")});
  QCOMPARE(changedSpy.size(), 0);
  QCOMPARE(reprojectedSpy.size(), 0);
}

void TaskListAppletControllerTests::reorderTaskFencesStaleRevisionAndUnknownIds() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  publishReady(fixture,
               {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1")),
                TaskListTest::standalone(QStringLiteral("w2"), QStringLiteral("app.2")),
                TaskListTest::standalone(QStringLiteral("w3"), QStringLiteral("app.3"))});
  const quint64 revision = fixture.source.revision();
  QSignalSpy committedSpy(
      &controller, &TaskListAppletController::taskOrderCommitted);

  // A stale revision is refused exactly like every other intent: the user
  // can only reorder the generation they actually see.
  QVERIFY(!controller.reorderTask(QStringLiteral("w2"), QString{},
                                  revision + 1));
  QVERIFY(controller.feedbackPresent());
  QCOMPARE(committedSpy.size(), 0);
  controller.clearFeedback();

  // Unknown moved/drop targets are refused without touching state.
  QVERIFY(!controller.reorderTask(QStringLiteral("w-gone"), QString{},
                                  revision));
  QVERIFY(!controller.reorderTask(QStringLiteral("w1"),
                                  QStringLiteral("w-gone"), revision));
  QVERIFY(!controller.reorderTask(QStringLiteral("w1"), QStringLiteral("w1"),
                                  revision));
  QCOMPARE(controller.entryRows().at(0).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w1")));
  QCOMPARE(committedSpy.size(), 0);

  // Read denial refuses reorder before anything else.
  ControllerFixture deniedFixture;
  TaskListAppletController denied(deniedFixture.source, deniedFixture.authority,
                                  deniedFixture.port, {false, true, true});
  publishReady(deniedFixture,
               {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1"))});
  QVERIFY(!denied.reorderTask(QStringLiteral("w1"), QString{}, 1));
  QVERIFY(denied.feedbackPresent());
}

void TaskListAppletControllerTests::reorderTaskCommitsAndPersistsAcrossGenerations() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  publishReady(fixture,
               {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1")),
                TaskListTest::standalone(QStringLiteral("w2"), QStringLiteral("app.2")),
                TaskListTest::standalone(QStringLiteral("w3"), QStringLiteral("app.3"))});
  const quint64 revision = fixture.source.revision();
  QSignalSpy committedSpy(
      &controller, &TaskListAppletController::taskOrderCommitted);

  // Drag w3 into the gap before w2: the committed order is the exact list to
  // persist, and the displayed order follows.
  QVERIFY(controller.reorderTask(QStringLiteral("w3"),
                                 QStringLiteral("w2"), revision));
  QCOMPARE(committedSpy.size(), 1);
  QCOMPARE(committedSpy.first().at(0).value<QStringList>(),
           (QStringList{QStringLiteral("w1"), QStringLiteral("w3"),
                        QStringLiteral("w2")}));
  QCOMPARE(controller.userTaskOrder(),
           (QStringList{QStringLiteral("w1"), QStringLiteral("w3"),
                        QStringLiteral("w2")}));
  const auto rows = controller.entryRows();
  QCOMPARE(rows.at(1).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w3")));
  QCOMPARE(rows.at(1).toMap().value(QStringLiteral("keyboardIndex")),
           QVariant(2));

  // A new generation (w4 arrives) re-projects canonically; the stored
  // overlay still wins for the known ids and w4 joins at its canonical tail.
  publishReady(fixture,
               {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1")),
                TaskListTest::standalone(QStringLiteral("w2"), QStringLiteral("app.2")),
                TaskListTest::standalone(QStringLiteral("w3"), QStringLiteral("app.3")),
                TaskListTest::standalone(QStringLiteral("w4"), QStringLiteral("app.4"))});
  QCOMPARE(controller.entryRows().at(0).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w1")));
  QCOMPARE(controller.entryRows().at(1).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w3")));
  QCOMPARE(controller.entryRows().at(2).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w2")));
  QCOMPARE(controller.entryRows().at(3).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w4")));

  // Append semantics against the fresh generation: an empty drop target
  // moves the task to the end. The stored order is the full displayed order,
  // so w4 joins it too.
  const quint64 nextRevision = fixture.source.revision();
  QVERIFY(controller.reorderTask(QStringLiteral("w1"), QString{}, nextRevision));
  QCOMPARE(controller.userTaskOrder(),
           (QStringList{QStringLiteral("w3"), QStringLiteral("w2"),
                        QStringLiteral("w4"), QStringLiteral("w1")}));
  QCOMPARE(controller.entryRows().at(3).toMap().value(QStringLiteral("taskId")),
           QVariant(QStringLiteral("w1")));
  QCOMPARE(controller.entryRows().at(3).toMap().value(QStringLiteral("keyboardIndex")),
           QVariant(4));
}

void TaskListAppletControllerTests::previewRequestsAreFencedBoundedAndSuperseded() {
  ControllerFixture fixture;
  TaskListAppletController controller(fixture.source, fixture.authority,
                                      fixture.port, allGrants());
  // No port: previews are disabled and requests are inert no-ops.
  QCOMPARE(controller.previewsEnabled(), false);
  controller.requestTaskPreview(QStringLiteral("w1"), 1, 320, 200);

  FakePreviewPort previewPort;
  controller.setPreviewPort(&previewPort);
  QCOMPARE(controller.previewsEnabled(), true);
  publishReady(fixture,
               {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1")),
                TaskListTest::standalone(QStringLiteral("w2"), QStringLiteral("app.2"))});
  const quint64 revision = fixture.source.revision();

  QQmlEngine engine;
  controller.installPreviewProvider(&engine);

  QSignalSpy arrivedSpy(&controller,
                        &TaskListAppletController::previewArrived);
  controller.requestTaskPreview(QStringLiteral("w1"), revision, 320, 200);
  QCOMPARE(previewPort.calls.size(), 1);
  QCOMPARE(previewPort.calls.last().windowId, QStringLiteral("w1"));
  QCOMPARE(previewPort.calls.last().revision, revision);
  QCOMPARE(previewPort.calls.last().maxSize, QSize(320, 200));

  // A stale revision request never reaches the port.
  controller.requestTaskPreview(QStringLiteral("w1"), revision + 1, 320, 200);
  QCOMPARE(previewPort.calls.size(), 1);

  // An unavailable capture surfaces as a zero token (tooltip fallback).
  previewPort.replyOk = false;
  previewPort.autoReply = true;
  controller.requestTaskPreview(QStringLiteral("w2"), revision, 320, 200);
  QCOMPARE(arrivedSpy.size(), 1);
  QCOMPARE(arrivedSpy.at(0).at(0).toString(), QStringLiteral("w2"));
  QCOMPARE(arrivedSpy.at(0).at(2).toInt(), 0);

  // A capture for a superseded request is dropped: only the newest
  // (taskId, revision) pair may decorate the strip.
  arrivedSpy.clear();
  previewPort.autoReply = false;
  previewPort.replyImage = QImage(64, 48, QImage::Format_ARGB32);
  controller.requestTaskPreview(QStringLiteral("w1"), revision, 320, 200);
  const auto superseded = previewPort.calls.takeLast();
  controller.requestTaskPreview(QStringLiteral("w2"), revision, 320, 200);
  Q_EMIT previewPort.previewFinished(
      TaskListAppletTest::FakePreviewPort::makeResult(
          superseded.windowId, superseded.revision, true,
          QImage(64, 48, QImage::Format_ARGB32)));
  QCOMPARE(arrivedSpy.size(), 0);
  Q_EMIT previewPort.previewFinished(
      TaskListAppletTest::FakePreviewPort::makeResult(
          QStringLiteral("w2"), revision, true,
          QImage(64, 48, QImage::Format_ARGB32)));
  QCOMPARE(arrivedSpy.size(), 1);
  QVERIFY(arrivedSpy.at(0).at(2).toInt() > 0);

  // Cancel drops the in-flight attribution.
  controller.requestTaskPreview(QStringLiteral("w1"), revision, 320, 200);
  controller.cancelTaskPreview();
  arrivedSpy.clear();
  Q_EMIT previewPort.previewFinished(
      TaskListAppletTest::FakePreviewPort::makeResult(
          QStringLiteral("w1"), revision, true,
          QImage(64, 48, QImage::Format_ARGB32)));
  QCOMPARE(arrivedSpy.size(), 0);
}

QTEST_GUILESS_MAIN(TaskListAppletControllerTests)
#include "tst_task_list_applet_controller.moc"
