// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/task_list_order.h"

#include <QtTest>

using namespace QindaQt::ShellTaskList;

namespace {

TaskEntry entryOf(const QString &taskId, const QString &applicationId) {
  TaskEntry entry;
  entry.taskId = taskId;
  entry.kind = TaskEntryKind::Window;
  entry.applicationId = applicationId;
  entry.primaryWindowId = taskId;
  return entry;
}

TaskListPresentation presentationOf(const QStringList &taskIds) {
  TaskListPresentation presentation;
  presentation.state = TaskListState::Ready;
  int index = 1;
  for (const QString &taskId : taskIds) {
    presentation.entries.append(entryOf(taskId, QStringLiteral("app")));
    presentation.identities.append(
        TaskEntryIdentity{taskId, index, QStringLiteral("Accessible %1").arg(taskId)});
    ++index;
  }
  return presentation;
}

QStringList idsOf(const TaskListPresentation &presentation) {
  QStringList ids;
  ids.reserve(presentation.entries.size());
  for (const TaskEntry &entry : presentation.entries) {
    ids.append(entry.taskId);
  }
  return ids;
}

} // namespace

class TaskListOrderTests final : public QObject {
  Q_OBJECT

private slots:
  void normalizeDropsBlankOversizedAndDuplicateIds();
  void normalizeBoundsTheListAtTheFactCeiling();
  void emptyOverlayKeepsCanonicalOrder();
  void overlayMovesNamedEntriesAheadOfCanonicalTail();
  void unknownOverlayIdsAreIgnored();
  void keyboardIndicesFollowTheDisplayedOrder();
  void overlaySurvivesAcrossReprojections();
};

void TaskListOrderTests::normalizeDropsBlankOversizedAndDuplicateIds() {
  QString oversized;
  oversized.fill(QLatin1Char('x'), int(kMaxIdLength) + 1);
  const QStringList normalized = TaskListOrder::normalize(
      {QStringLiteral(" w-1 "), QString{}, QStringLiteral("w-1"),
       oversized, QStringLiteral("w-2")});
  QCOMPARE(normalized, (QStringList{QStringLiteral("w-1"),
                                    QStringLiteral("w-2")}));
}

void TaskListOrderTests::normalizeBoundsTheListAtTheFactCeiling() {
  QStringList flood;
  flood.reserve(int(kMaxWindowFacts) + 10);
  for (int index = 0; index < int(kMaxWindowFacts) + 10; ++index) {
    flood.append(QStringLiteral("w-%1").arg(index));
  }
  QCOMPARE(TaskListOrder::normalize(flood).size(), int(kMaxWindowFacts));
}

void TaskListOrderTests::emptyOverlayKeepsCanonicalOrder() {
  TaskListPresentation presentation =
      presentationOf({QStringLiteral("w-1"), QStringLiteral("w-2"),
                      QStringLiteral("w-3")});
  TaskListOrder::applyOverlay({}, presentation.entries,
                              presentation.identities);
  QCOMPARE(idsOf(presentation), (QStringList{QStringLiteral("w-1"),
                                             QStringLiteral("w-2"),
                                             QStringLiteral("w-3")}));
}

void TaskListOrderTests::overlayMovesNamedEntriesAheadOfCanonicalTail() {
  TaskListPresentation presentation =
      presentationOf({QStringLiteral("w-1"), QStringLiteral("w-2"),
                      QStringLiteral("w-3"), QStringLiteral("w-4")});
  // A persisted drag: w-3 before w-1, everything else keeps canonical order.
  TaskListOrder::applyOverlay({QStringLiteral("w-3")}, presentation.entries,
                              presentation.identities);
  QCOMPARE(idsOf(presentation), (QStringList{QStringLiteral("w-3"),
                                             QStringLiteral("w-1"),
                                             QStringLiteral("w-2"),
                                             QStringLiteral("w-4")}));
}

void TaskListOrderTests::unknownOverlayIdsAreIgnored() {
  TaskListPresentation presentation =
      presentationOf({QStringLiteral("w-1"), QStringLiteral("w-2")});
  // Closed windows stay in the persisted order; they never reorder live rows.
  TaskListOrder::applyOverlay({QStringLiteral("w-gone"),
                               QStringLiteral("w-2")},
                              presentation.entries,
                              presentation.identities);
  QCOMPARE(idsOf(presentation), (QStringList{QStringLiteral("w-2"),
                                             QStringLiteral("w-1")}));
}

void TaskListOrderTests::keyboardIndicesFollowTheDisplayedOrder() {
  TaskListPresentation presentation =
      presentationOf({QStringLiteral("w-1"), QStringLiteral("w-2"),
                      QStringLiteral("w-3")});
  TaskListOrder::applyOverlay({QStringLiteral("w-3"), QStringLiteral("w-1")},
                              presentation.entries,
                              presentation.identities);
  QCOMPARE(idsOf(presentation), (QStringList{QStringLiteral("w-3"),
                                             QStringLiteral("w-1"),
                                             QStringLiteral("w-2")}));
  QCOMPARE(presentation.identities.at(0).keyboardIndex, 1);
  QCOMPARE(presentation.identities.at(1).keyboardIndex, 2);
  QCOMPARE(presentation.identities.at(2).keyboardIndex, 3);
  QCOMPARE(presentation.identities.at(2).accessibleName,
           QStringLiteral("Accessible w-2"));
}

void TaskListOrderTests::overlaySurvivesAcrossReprojections() {
  // A new generation re-projects canonically; applying the same overlay
  // again restores the exact displayed order.
  const QStringList overlay{QStringLiteral("w-2"), QStringLiteral("w-1")};
  TaskListPresentation first = presentationOf(
      {QStringLiteral("w-1"), QStringLiteral("w-2"), QStringLiteral("w-3")});
  TaskListOrder::applyOverlay(overlay, first.entries, first.identities);
  QCOMPARE(idsOf(first), (QStringList{QStringLiteral("w-2"),
                                      QStringLiteral("w-1"),
                                      QStringLiteral("w-3")}));

  TaskListPresentation second = presentationOf(
      {QStringLiteral("w-1"), QStringLiteral("w-2"), QStringLiteral("w-3"),
       QStringLiteral("w-4")});
  TaskListOrder::applyOverlay(overlay, second.entries, second.identities);
  QCOMPARE(idsOf(second), (QStringList{QStringLiteral("w-2"),
                                       QStringLiteral("w-1"),
                                       QStringLiteral("w-3"),
                                       QStringLiteral("w-4")}));
}

QTEST_GUILESS_MAIN(TaskListOrderTests)
#include "tst_task_list_order.moc"
