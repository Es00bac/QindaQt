// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_fact_joiner.h"

#include "qindaqt/shell/task_list/task_list_source.h"

#include <QtTest>

#include "task_list_producer_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

namespace {

TaskListJoinResult joinScene(const StandardScene &scene) {
  // Decoder correctness is covered by qindaqt.task-list-wire; these decodes
  // are fixture plumbing and cannot fail for a builder-produced scene.
  const auto windows = TaskListWireDecoder::decodeWindows(scene.windows);
  const auto containers = TaskListWireDecoder::decodeContainers(scene.containers);
  const auto scope = TaskListWireDecoder::decodeScopeSnapshot(scene.scope);
  return TaskListFactJoiner::join(windows.windows, containers.containers,
                                  scope.snapshot);
}

TaskListWireWindow wireWindow(const QString &id, const QString &app,
                              const QString &containerId = {},
                              bool skipTaskbar = false) {
  TaskListWireWindow window;
  window.windowId = id;
  window.applicationId = app;
  window.title = QStringLiteral("Title %1").arg(id);
  window.containerId = containerId;
  window.skipTaskbar = skipTaskbar;
  return window;
}

TaskListWireScope wireScope(const QString &id) {
  TaskListWireScope scope;
  scope.windowId = id;
  scope.outputId = QStringLiteral("output-1");
  scope.workspaceIds = {QStringLiteral("ws-1")};
  return scope;
}

} // namespace

class TaskListFactJoinerTests final : public QObject {
  Q_OBJECT

private slots:
  void joinsCoherentSceneIntoValidatedFacts();
  void dropsStandaloneSkipTaskbarWindows();
  void memberScopeFollowsThePrimary();
  void rejectsMissingScope();
  void rejectsUnknownContainerOwner();
  void rejectsEmptyContainerOnWire();
  void rejectsMissingNativeIdentity();
  void rejectsAmbiguousNativeIdentity();
};

void TaskListFactJoinerTests::joinsCoherentSceneIntoValidatedFacts() {
  const TaskListJoinResult joined = joinScene(standardScene());
  QVERIFY2(joined.ok(), qPrintable(joined.error.message));
  QCOMPARE(joined.facts.size(), 3);
  QCOMPARE(joined.containers.size(), 1);
  QCOMPARE(joined.containers.constFirst().containerId, QStringLiteral("c1"));
  QCOMPARE(joined.containers.constFirst().revision, quint64(7));
  QCOMPARE(joined.containers.constFirst().authority,
           TaskListContainerAuthority::HybridProcess);

  int primaries = 0;
  int members = 0;
  for (const TaskWindowFact &fact : joined.facts) {
    // Compositor1 1.1 exposes no urgent state; every fact must say so.
    QVERIFY(!fact.urgent);
    // No display name exists on the wire; the application id is the fallback.
    QCOMPARE(fact.applicationName, fact.applicationId);
    QCOMPARE(fact.outputId, QStringLiteral("output-1"));
    if (fact.role == TaskWindowRole::ContainerPrimary) {
      ++primaries;
      QCOMPARE(fact.windowId, QStringLiteral("w2"));
      QCOMPARE(fact.containerId, QStringLiteral("c1"));
    } else if (fact.role == TaskWindowRole::ContainerMember) {
      ++members;
      QCOMPARE(fact.windowId, QStringLiteral("w3"));
      QVERIFY(!fact.active);
      QVERIFY(!fact.minimized);
    }
  }
  QCOMPARE(primaries, 1);
  QCOMPARE(members, 1);

  // The T0 model is the second enforcement point (ADR-0044): the join must
  // satisfy the full batch validation, not only the joiner's own rules.
  TaskListSource source;
  const TaskListEvaluation evaluation = source.publishGeneration(joined.facts);
  QVERIFY2(evaluation.ok(), qPrintable(evaluation.error.message));
  QCOMPARE(evaluation.generation.entries.size(), 2);
  QCOMPARE(evaluation.generation.entries.at(1).windowCount, quint32(2));
}

void TaskListFactJoinerTests::dropsStandaloneSkipTaskbarWindows() {
  const QVector<TaskListWireWindow> windows{
      wireWindow(QStringLiteral("w1"), QStringLiteral("app.one")),
      wireWindow(QStringLiteral("w2"), QStringLiteral("app.two"), {}, true)};
  TaskListWireScopeSnapshot scope;
  scope.epoch = QStringLiteral("e");
  scope.revision = 1;
  scope.scopes = {wireScope(QStringLiteral("w1")),
                  wireScope(QStringLiteral("w2"))};

  const TaskListJoinResult joined =
      TaskListFactJoiner::join(windows, {}, scope);
  QVERIFY2(joined.ok(), qPrintable(joined.error.message));
  QCOMPARE(joined.facts.size(), 1);
  QCOMPARE(joined.facts.constFirst().windowId, QStringLiteral("w1"));
  QCOMPARE(joined.facts.constFirst().role, TaskWindowRole::Standalone);
}

void TaskListFactJoinerTests::memberScopeFollowsThePrimary() {
  const QVector<TaskListWireWindow> windows{
      wireWindow(QStringLiteral("w2"), QStringLiteral("app.two"),
                 QStringLiteral("c1")),
      wireWindow(QStringLiteral("w3"), QStringLiteral("app.three"),
                 QStringLiteral("c1"), true)};
  const QVector<TaskListWireContainer> containers{
      {QStringLiteral("c1"), 3, TaskListContainerAuthority::ControlBridge}};
  TaskListWireScopeSnapshot scope;
  scope.epoch = QStringLiteral("e");
  scope.revision = 1;
  TaskListWireScope primaryScope = wireScope(QStringLiteral("w2"));
  primaryScope.outputId = QStringLiteral("output-9");
  primaryScope.workspaceIds = {QStringLiteral("ws-9")};
  // The suppressed member's own scope entry disagrees; the primary's placement
  // must win because it describes the whole container.
  TaskListWireScope memberScope = wireScope(QStringLiteral("w3"));
  memberScope.outputId = QStringLiteral("output-3");
  memberScope.workspaceIds = {QStringLiteral("ws-3")};
  scope.scopes = {primaryScope, memberScope};

  const TaskListJoinResult joined =
      TaskListFactJoiner::join(windows, containers, scope);
  QVERIFY2(joined.ok(), qPrintable(joined.error.message));
  QCOMPARE(joined.facts.size(), 2);
  for (const TaskWindowFact &fact : joined.facts) {
    QCOMPARE(fact.outputId, QStringLiteral("output-9"));
    QCOMPARE(fact.workspaceIds, QStringList{QStringLiteral("ws-9")});
  }
}

void TaskListFactJoinerTests::rejectsMissingScope() {
  const QVector<TaskListWireWindow> windows{
      wireWindow(QStringLiteral("w1"), QStringLiteral("app.one"))};
  TaskListWireScopeSnapshot scope;
  scope.epoch = QStringLiteral("e");
  scope.revision = 1;
  const TaskListJoinResult joined =
      TaskListFactJoiner::join(windows, {}, scope);
  QCOMPARE(joined.error.code, TaskListJoinError::ScopeUnavailable);
  QCOMPARE(joined.error.windowId, QStringLiteral("w1"));
}

void TaskListFactJoinerTests::rejectsUnknownContainerOwner() {
  const QVector<TaskListWireWindow> windows{
      wireWindow(QStringLiteral("w2"), QStringLiteral("app.two"),
                 QStringLiteral("c9"))};
  TaskListWireScopeSnapshot scope;
  scope.epoch = QStringLiteral("e");
  scope.revision = 1;
  scope.scopes = {wireScope(QStringLiteral("w2"))};
  const TaskListJoinResult joined =
      TaskListFactJoiner::join(windows, {}, scope);
  QCOMPARE(joined.error.code, TaskListJoinError::UnknownContainerOwner);
  QCOMPARE(joined.error.containerId, QStringLiteral("c9"));
}

void TaskListFactJoinerTests::rejectsEmptyContainerOnWire() {
  const QVector<TaskListWireWindow> windows{
      wireWindow(QStringLiteral("w1"), QStringLiteral("app.one"))};
  const QVector<TaskListWireContainer> containers{
      {QStringLiteral("c1"), 1, TaskListContainerAuthority::HybridProcess}};
  TaskListWireScopeSnapshot scope;
  scope.epoch = QStringLiteral("e");
  scope.revision = 1;
  scope.scopes = {wireScope(QStringLiteral("w1"))};
  const TaskListJoinResult joined =
      TaskListFactJoiner::join(windows, containers, scope);
  QCOMPARE(joined.error.code, TaskListJoinError::EmptyContainerOnWire);
  QCOMPARE(joined.error.containerId, QStringLiteral("c1"));
}

void TaskListFactJoinerTests::rejectsMissingNativeIdentity() {
  const QVector<TaskListWireWindow> windows{
      wireWindow(QStringLiteral("w2"), QStringLiteral("app.two"),
                 QStringLiteral("c1"), true),
      wireWindow(QStringLiteral("w3"), QStringLiteral("app.three"),
                 QStringLiteral("c1"), true)};
  const QVector<TaskListWireContainer> containers{
      {QStringLiteral("c1"), 1, TaskListContainerAuthority::HybridProcess}};
  TaskListWireScopeSnapshot scope;
  scope.epoch = QStringLiteral("e");
  scope.revision = 1;
  scope.scopes = {wireScope(QStringLiteral("w2")),
                  wireScope(QStringLiteral("w3"))};
  const TaskListJoinResult joined =
      TaskListFactJoiner::join(windows, containers, scope);
  QCOMPARE(joined.error.code, TaskListJoinError::TaskIdentityMissing);
}

void TaskListFactJoinerTests::rejectsAmbiguousNativeIdentity() {
  const QVector<TaskListWireWindow> windows{
      wireWindow(QStringLiteral("w2"), QStringLiteral("app.two"),
                 QStringLiteral("c1")),
      wireWindow(QStringLiteral("w3"), QStringLiteral("app.three"),
                 QStringLiteral("c1"))};
  const QVector<TaskListWireContainer> containers{
      {QStringLiteral("c1"), 1, TaskListContainerAuthority::HybridProcess}};
  TaskListWireScopeSnapshot scope;
  scope.epoch = QStringLiteral("e");
  scope.revision = 1;
  scope.scopes = {wireScope(QStringLiteral("w2")),
                  wireScope(QStringLiteral("w3"))};
  const TaskListJoinResult joined =
      TaskListFactJoiner::join(windows, containers, scope);
  QCOMPARE(joined.error.code, TaskListJoinError::TaskIdentityAmbiguous);
}

QTEST_GUILESS_MAIN(TaskListFactJoinerTests)
#include "tst_task_list_fact_joiner.moc"
