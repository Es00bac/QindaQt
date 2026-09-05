// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/workspaces/workspace_types.h"

#include <QtTest>

using namespace QindaQt::Shell::Workspaces;

namespace {

WorkspaceSnapshot threeDesktops()
{
  WorkspaceSnapshot snapshot;
  snapshot.desktops = {{2, QStringLiteral("c"), QStringLiteral("Three")},
                       {0, QStringLiteral("a"), QStringLiteral("One")},
                       {1, QStringLiteral("b"), QStringLiteral("Two")}};
  snapshot.currentId = QStringLiteral("b");
  snapshot.rows = 1;
  return snapshot;
}

} // namespace

class WorkspaceTypesTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void validSnapshotIsSortedByPosition();
  void rejectsEmptyAndOversizedLists();
  void rejectsInvalidIdsAndUnknownCurrent();
  void truncatesLongNamesInsteadOfRejecting();
  void phaseTextsAreStable();
};

void WorkspaceTypesTests::validSnapshotIsSortedByPosition()
{
  const WorkspaceValidation validation = validateWorkspaceSnapshot(threeDesktops());
  QVERIFY2(validation.ok, qPrintable(validation.reasonCode));
  QCOMPARE(validation.snapshot.desktops.size(), 3);
  QCOMPARE(validation.snapshot.desktops.at(0).id, QStringLiteral("a"));
  QCOMPARE(validation.snapshot.desktops.at(1).id, QStringLiteral("b"));
  QCOMPARE(validation.snapshot.desktops.at(2).id, QStringLiteral("c"));
  QCOMPARE(validation.snapshot.currentId, QStringLiteral("b"));
}

void WorkspaceTypesTests::rejectsEmptyAndOversizedLists()
{
  WorkspaceSnapshot empty;
  empty.currentId = QStringLiteral("a");
  QCOMPARE(validateWorkspaceSnapshot(empty).reasonCode, QStringLiteral("no-desktops"));

  WorkspaceSnapshot oversized;
  for (int index = 0; index <= Bounds::maxWorkspaces; ++index) {
    oversized.desktops.append({index, QStringLiteral("d%1").arg(index), {}});
  }
  oversized.currentId = QStringLiteral("d0");
  QCOMPARE(validateWorkspaceSnapshot(oversized).reasonCode,
           QStringLiteral("too-many-desktops"));

  WorkspaceSnapshot zeroRows = threeDesktops();
  zeroRows.rows = 0;
  QCOMPARE(validateWorkspaceSnapshot(zeroRows).reasonCode, QStringLiteral("zero-rows"));
}

void WorkspaceTypesTests::rejectsInvalidIdsAndUnknownCurrent()
{
  WorkspaceSnapshot duplicate = threeDesktops();
  duplicate.desktops[2].id = QStringLiteral("a");
  QCOMPARE(validateWorkspaceSnapshot(duplicate).reasonCode,
           QStringLiteral("duplicate-desktop-id"));

  WorkspaceSnapshot emptyId = threeDesktops();
  emptyId.desktops[0].id.clear();
  QCOMPARE(validateWorkspaceSnapshot(emptyId).reasonCode,
           QStringLiteral("invalid-desktop-id"));

  WorkspaceSnapshot longId = threeDesktops();
  longId.desktops[0].id = QString(Bounds::maxIdLength + 1, QLatin1Char('x'));
  QCOMPARE(validateWorkspaceSnapshot(longId).reasonCode,
           QStringLiteral("invalid-desktop-id"));

  WorkspaceSnapshot negative = threeDesktops();
  negative.desktops[0].position = -1;
  QCOMPARE(validateWorkspaceSnapshot(negative).reasonCode,
           QStringLiteral("negative-position"));

  WorkspaceSnapshot unknownCurrent = threeDesktops();
  unknownCurrent.currentId = QStringLiteral("zzz");
  QCOMPARE(validateWorkspaceSnapshot(unknownCurrent).reasonCode,
           QStringLiteral("current-desktop-unknown"));
}

void WorkspaceTypesTests::truncatesLongNamesInsteadOfRejecting()
{
  WorkspaceSnapshot longName = threeDesktops();
  longName.desktops[0].name = QString(Bounds::maxNameLength + 40, QLatin1Char('n'));
  const WorkspaceValidation validation = validateWorkspaceSnapshot(longName);
  QVERIFY(validation.ok);
  // Validation sorts by compositor position, so the edited row (position 2)
  // is the last item in the normalized snapshot.
  QCOMPARE(validation.snapshot.desktops.at(2).name.size(), Bounds::maxNameLength);
}

void WorkspaceTypesTests::phaseTextsAreStable()
{
  QCOMPARE(workspacePhaseText(WorkspacePhase::Loading), QStringLiteral("loading"));
  QCOMPARE(workspacePhaseText(WorkspacePhase::Ready), QStringLiteral("ready"));
  QCOMPARE(workspacePhaseText(WorkspacePhase::Degraded), QStringLiteral("degraded"));
  QCOMPARE(workspacePhaseText(WorkspacePhase::Unavailable), QStringLiteral("unavailable"));
}

QTEST_GUILESS_MAIN(WorkspaceTypesTests)
#include "tst_workspace_types.moc"
