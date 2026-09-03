// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/task_list_applet_projection.h"

#include <QtTest>

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskListApplet;

namespace {

const QString kAppOne = QStringLiteral("app.one");
const QString kAppTwo = QStringLiteral("app.two");

TaskEntry makeEntry(const QString &taskId, const QString &applicationId,
                    TaskEntryKind kind = TaskEntryKind::Window) {
  TaskEntry entry;
  entry.taskId = taskId;
  entry.kind = kind;
  entry.applicationId = applicationId;
  entry.applicationName = QStringLiteral("App %1").arg(applicationId);
  entry.title = QStringLiteral("Title %1").arg(taskId);
  entry.primaryWindowId = taskId;
  entry.outputId = QStringLiteral("output-1");
  entry.workspaceIds = {QStringLiteral("ws-1")};
  return entry;
}

TaskListPresentation presentationOf(TaskListState state,
                                    const QVector<TaskEntry> &entries) {
  TaskListPresentation presentation;
  presentation.state = state;
  presentation.entries = entries;
  for (int index = 0; index < entries.size(); ++index) {
    TaskEntryIdentity identity;
    identity.taskId = entries.at(index).taskId;
    identity.keyboardIndex = index + 1;
    identity.accessibleName =
        QStringLiteral("Accessible %1").arg(entries.at(index).taskId);
    presentation.identities.append(identity);
  }
  return presentation;
}

} // namespace

class TaskListAppletProjectionTests final : public QObject {
  Q_OBJECT

private slots:
  void readDenialWithholdsEverything();
  void loadingAndEmptyPhasesCarryReasonsAndNoRows();
  void readyPhaseCarriesNoReason();
  void degradedPhaseKeepsRetainedRows();
  void overflowTruthIsExactAndKeepsTheCanonicalHead();
  void rowFieldsMirrorTheEntryAndStampTheRevision();
  void pendingMarkersFlagOnlyNamedTasks();
  void iconPlaceholderIsDeterministicWithFallbacks();
  void projectionIsDeterministic();
  void phaseTextCoversEveryPhase();
};

void TaskListAppletProjectionTests::readDenialWithholdsEverything() {
  const auto presentation = presentationOf(
      TaskListState::Ready, {makeEntry(QStringLiteral("w-1"), kAppOne)});
  const auto projection = TaskListAppletProjectionModel::project(
      presentation, {}, false, quint64(9));
  QCOMPARE(projection.phase, TaskListAppletPhase::Unavailable);
  QCOMPARE(projection.phaseReason,
           QStringLiteral("windows-read-not-granted"));
  QCOMPARE(projection.rows.isEmpty(), true);
  QCOMPARE(projection.totalCount, 0);
  QCOMPARE(projection.overflowCount, 0);
}

void TaskListAppletProjectionTests::loadingAndEmptyPhasesCarryReasonsAndNoRows() {
  // Loading must ignore whatever generation was handed in.
  const auto loading = TaskListAppletProjectionModel::project(
      presentationOf(TaskListState::Loading,
                     {makeEntry(QStringLiteral("w-1"), kAppOne)}),
      {}, true, quint64(0));
  QCOMPARE(loading.phase, TaskListAppletPhase::Loading);
  QCOMPARE(loading.phaseReason,
           QStringLiteral("compositor-task-list-loading"));
  QCOMPARE(loading.rows.isEmpty(), true);

  const auto empty = TaskListAppletProjectionModel::project(
      presentationOf(TaskListState::Empty, {}), {}, true, quint64(3));
  QCOMPARE(empty.phase, TaskListAppletPhase::Empty);
  QCOMPARE(empty.phaseReason, QStringLiteral("no-windows-in-scope"));
  QCOMPARE(empty.rows.isEmpty(), true);
  QCOMPARE(empty.totalCount, 0);
  QCOMPARE(empty.overflowCount, 0);
}

void TaskListAppletProjectionTests::readyPhaseCarriesNoReason() {
  const auto projection = TaskListAppletProjectionModel::project(
      presentationOf(TaskListState::Ready,
                     {makeEntry(QStringLiteral("w-1"), kAppOne)}),
      {}, true, quint64(4));
  QCOMPARE(projection.phase, TaskListAppletPhase::Ready);
  QCOMPARE(projection.phaseReason.isEmpty(), true);
  QCOMPARE(projection.rows.size(), 1);
}

void TaskListAppletProjectionTests::degradedPhaseKeepsRetainedRows() {
  const auto projection = TaskListAppletProjectionModel::project(
      presentationOf(TaskListState::Degraded,
                     {makeEntry(QStringLiteral("w-1"), kAppOne),
                      makeEntry(QStringLiteral("w-2"), kAppTwo)}),
      {}, true, quint64(5));
  QCOMPARE(projection.phase, TaskListAppletPhase::Degraded);
  QCOMPARE(projection.phaseReason,
           QStringLiteral("compositor-task-list-unavailable"));
  QCOMPARE(projection.rows.size(), 2);
  QCOMPARE(projection.rows.at(0).taskId, QStringLiteral("w-1"));
  QCOMPARE(projection.rows.at(1).taskId, QStringLiteral("w-2"));
  QCOMPARE(projection.totalCount, 2);
}

void TaskListAppletProjectionTests::overflowTruthIsExactAndKeepsTheCanonicalHead() {
  QVector<TaskEntry> entries;
  entries.reserve(kMaxPresentedTaskEntries + 6);
  for (int index = 0; index < kMaxPresentedTaskEntries + 6; ++index) {
    entries.append(makeEntry(QStringLiteral("w-%1").arg(index, 4, 10,
                                                         QLatin1Char('0')),
                             kAppOne));
  }
  const auto projection = TaskListAppletProjectionModel::project(
      presentationOf(TaskListState::Ready, entries), {}, true, quint64(7));
  QCOMPARE(projection.rows.size(), kMaxPresentedTaskEntries);
  QCOMPARE(projection.totalCount, kMaxPresentedTaskEntries + 6);
  QCOMPARE(projection.overflowCount, 6);
  // The cap truncates the tail only: the canonical head is always retained.
  QCOMPARE(projection.rows.constFirst().taskId, QStringLiteral("w-0000"));
  QCOMPARE(projection.rows.constLast().taskId, QStringLiteral("w-0063"));
  QCOMPARE(projection.rows.constLast().keyboardIndex,
           kMaxPresentedTaskEntries);
}

void TaskListAppletProjectionTests::rowFieldsMirrorTheEntryAndStampTheRevision() {
  auto container = makeEntry(QStringLiteral("c-1"), kAppTwo,
                             TaskEntryKind::Container);
  container.primaryWindowId = QStringLiteral("w-2");
  container.memberWindowIds = {QStringLiteral("w-2"), QStringLiteral("w-3")};
  container.windowCount = 2;
  container.active = true;
  auto window = makeEntry(QStringLiteral("w-1"), kAppOne);
  window.minimized = true;
  window.urgent = true;

  const auto projection = TaskListAppletProjectionModel::project(
      presentationOf(TaskListState::Ready, {window, container}), {}, true,
      quint64(42));
  QCOMPARE(projection.rows.size(), 2);

  const TaskListAppletRow &windowRow = projection.rows.at(0);
  QCOMPARE(windowRow.taskId, QStringLiteral("w-1"));
  QCOMPARE(windowRow.kind, TaskEntryKind::Window);
  QCOMPARE(windowRow.title, QStringLiteral("Title w-1"));
  QCOMPARE(windowRow.applicationId, kAppOne);
  QCOMPARE(windowRow.applicationName, QStringLiteral("App %1").arg(kAppOne));
  QCOMPARE(windowRow.iconText, QStringLiteral("A"));
  QCOMPARE(windowRow.windowCount, quint32(1));
  QCOMPARE(windowRow.active, false);
  QCOMPARE(windowRow.minimized, true);
  QCOMPARE(windowRow.urgent, true);
  QCOMPARE(windowRow.keyboardIndex, 1);
  QCOMPARE(windowRow.accessibleName, QStringLiteral("Accessible w-1"));
  QCOMPARE(windowRow.generationRevision, quint64(42));
  QCOMPARE(windowRow.pending, false);
  QCOMPARE(windowRow.memberWindowIds.isEmpty(), true);

  const TaskListAppletRow &containerRow = projection.rows.at(1);
  QCOMPARE(containerRow.kind, TaskEntryKind::Container);
  QCOMPARE(containerRow.windowCount, quint32(2));
  QCOMPARE(containerRow.active, true);
  QCOMPARE(containerRow.memberWindowIds,
           (QStringList{QStringLiteral("w-2"), QStringLiteral("w-3")}));
  QCOMPARE(containerRow.generationRevision, quint64(42));
  QCOMPARE(containerRow.keyboardIndex, 2);
}

void TaskListAppletProjectionTests::pendingMarkersFlagOnlyNamedTasks() {
  const auto presentation = presentationOf(
      TaskListState::Ready,
      {makeEntry(QStringLiteral("w-1"), kAppOne),
       makeEntry(QStringLiteral("w-2"), kAppTwo)});
  const auto projection = TaskListAppletProjectionModel::project(
      presentation, {QStringLiteral("w-2")}, true, quint64(8));
  QCOMPARE(projection.rows.at(0).pending, false);
  QCOMPARE(projection.rows.at(1).pending, true);
}

void TaskListAppletProjectionTests::iconPlaceholderIsDeterministicWithFallbacks() {
  using Model = TaskListAppletProjectionModel;
  // The placeholder derives from the application identity, never the title.
  QCOMPARE(Model::iconPlaceholder(QStringLiteral("terminal"), {}),
           QStringLiteral("T"));
  QCOMPARE(Model::iconPlaceholder(QStringLiteral("42zip"), {}),
           QStringLiteral("4"));
  QCOMPARE(Model::iconPlaceholder(QStringLiteral("  spaced"), {}),
           QStringLiteral("S"));
  QCOMPARE(Model::iconPlaceholder({}, QStringLiteral("org.example.Idle")),
           QStringLiteral("O"));
  QCOMPARE(Model::iconPlaceholder({}, {}), QStringLiteral("?"));
  QCOMPARE(Model::iconPlaceholder(QStringLiteral("…"), {}),
           QStringLiteral("?"));
  QCOMPARE(Model::iconPlaceholder(QStringLiteral("Terminal"), {}),
           Model::iconPlaceholder(QStringLiteral("Terminal"), {}));
}

void TaskListAppletProjectionTests::projectionIsDeterministic() {
  const auto presentation = presentationOf(
      TaskListState::Ready,
      {makeEntry(QStringLiteral("w-1"), kAppOne),
       makeEntry(QStringLiteral("w-2"), kAppTwo)});
  const QSet<QString> pending{QStringLiteral("w-1")};
  const auto first = TaskListAppletProjectionModel::project(
      presentation, pending, true, quint64(11));
  const auto second = TaskListAppletProjectionModel::project(
      presentation, pending, true, quint64(11));
  QCOMPARE(first.phase, second.phase);
  QCOMPARE(first.phaseReason, second.phaseReason);
  QVERIFY(first.rows == second.rows);
  QCOMPARE(first.totalCount, second.totalCount);
  QCOMPARE(first.overflowCount, second.overflowCount);
}

void TaskListAppletProjectionTests::phaseTextCoversEveryPhase() {
  QCOMPARE(taskListAppletPhaseText(TaskListAppletPhase::Loading),
           QStringLiteral("loading"));
  QCOMPARE(taskListAppletPhaseText(TaskListAppletPhase::Ready),
           QStringLiteral("ready"));
  QCOMPARE(taskListAppletPhaseText(TaskListAppletPhase::Empty),
           QStringLiteral("empty"));
  QCOMPARE(taskListAppletPhaseText(TaskListAppletPhase::Degraded),
           QStringLiteral("degraded"));
  QCOMPARE(taskListAppletPhaseText(TaskListAppletPhase::Unavailable),
           QStringLiteral("unavailable"));
}

QTEST_GUILESS_MAIN(TaskListAppletProjectionTests)
#include "tst_task_list_applet_projection.moc"
