// SPDX-License-Identifier: GPL-3.0-or-later
#include "visibility_test_support.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::ShellVisibility;
using namespace QindaQt::ShellVisibility::TestSupport;

class PanelVisibilityScopeTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void windowsMustBelongToTheCurrentWorkspaceAndActivity();
  void allWorkspaceAndAllActivityWindowsAreRelevant();
  void minimizedAndHiddenWindowsDoNotParticipate();
  void spanningWindowsUseSurfaceOverlapAcrossOutputAssignments();
  void overlapUsesPanelSurfaceGeometryAndInclusivePixelBounds();
  void maximizedPolicyUsesOutputAssignmentRatherThanOverlap();
  void expandedPanelIdentityKeepsOutputDecisionsIndependent();
  void validEmptyPanelSetProducesAnEmptyDecisionBatch();
};

void PanelVisibilityScopeTest::
    windowsMustBelongToTheCurrentWorkspaceAndActivity() {
  auto snapshot = inventory(Profiles::HideMode::DodgeAll);
  auto candidate = window();
  candidate.workspaceId = QStringLiteral("workspace-2");
  snapshot.windows = {candidate};

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);

  snapshot.windows[0].workspaceId = QStringLiteral("workspace-1");
  snapshot.windows[0].activityIds = {QStringLiteral("activity-b")};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);

  snapshot.windows[0].activityIds = {QStringLiteral("activity-b"),
                                     QStringLiteral("activity-a")};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
}

void PanelVisibilityScopeTest::allWorkspaceAndAllActivityWindowsAreRelevant() {
  auto snapshot = inventory(Profiles::HideMode::DodgeAll);
  auto omnipresent = window();
  omnipresent.onAllWorkspaces = true;
  omnipresent.workspaceId.clear();
  omnipresent.activityIds.clear();
  snapshot.windows = {omnipresent};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("window"));
}

void PanelVisibilityScopeTest::minimizedAndHiddenWindowsDoNotParticipate() {
  auto snapshot = inventory(Profiles::HideMode::DodgeAll);
  auto minimized = window(QStringLiteral("minimized"));
  minimized.minimized = true;
  minimized.maximized = true;
  auto hidden = window(QStringLiteral("hidden"));
  hidden.hidden = true;
  snapshot.windows = {minimized, hidden};

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);

  snapshot.panels[0].hideMode = Profiles::HideMode::Maximized;
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
}

void PanelVisibilityScopeTest::
    spanningWindowsUseSurfaceOverlapAcrossOutputAssignments() {
  PanelVisibilityInventory snapshot;
  snapshot.outputs = {
      output(QStringLiteral("left"), QRect(-1920, 0, 1920, 1080)),
      output(QStringLiteral("right"), QRect(0, 0, 2560, 1440)),
  };
  snapshot.panels = {panel(Profiles::HideMode::DodgeAll,
                           QStringLiteral("right-top"), QStringLiteral("right"),
                           QRect(0, 0, 2560, 40))};
  auto spanning = window(QStringLiteral("spanning"), QRect(-100, 0, 200, 300),
                         QStringLiteral("left"));
  snapshot.windows = {spanning};
  snapshot.scope = {QStringLiteral("workspace-1"),
                    QStringLiteral("activity-a")};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::AnyWindowOverlap);
  QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("spanning"));
}

void PanelVisibilityScopeTest::
    overlapUsesPanelSurfaceGeometryAndInclusivePixelBounds() {
  auto snapshot = inventory(Profiles::HideMode::DodgeAll);
  snapshot.panels[0].surfaceGeometry = QRect(500, 0, 920, 32);
  snapshot.windows = {window(QStringLiteral("clear"), QRect(0, 0, 499, 300))};

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);

  // x=499..500 overlaps the actual centered surface by exactly one logical
  // pixel. The full output edge or work area is not used as a proxy.
  snapshot.windows[0].frameGeometry = QRect(499, 0, 2, 300);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);

  snapshot.panels[0].surfaceGeometry = QRect(0, 0, 1920, 32);
  snapshot.windows[0].frameGeometry = QRect(0, 32, 500, 400);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);

  snapshot.windows[0].frameGeometry = QRect(0, 31, 500, 400);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
}

void PanelVisibilityScopeTest::
    maximizedPolicyUsesOutputAssignmentRatherThanOverlap() {
  PanelVisibilityInventory snapshot;
  snapshot.outputs = {
      output(QStringLiteral("left"), QRect(-1280, 0, 1280, 1024)),
      output(QStringLiteral("right"), QRect(0, 0, 1920, 1080)),
  };
  snapshot.panels = {panel(Profiles::HideMode::Maximized,
                           QStringLiteral("left-panel"), QStringLiteral("left"),
                           QRect(-1280, 0, 1280, 32))};
  auto maximized = window(QStringLiteral("spanning-maximized"),
                          QRect(-20, 0, 900, 900), QStringLiteral("right"));
  maximized.maximized = true;
  snapshot.windows = {maximized};
  snapshot.scope = {QStringLiteral("workspace-1"),
                    QStringLiteral("activity-a")};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  // The frame intersects the left panel, but maximized mode belongs to the
  // compositor-selected output instead of behaving like dodge-all.
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
}

void PanelVisibilityScopeTest::
    expandedPanelIdentityKeepsOutputDecisionsIndependent() {
  PanelVisibilityInventory snapshot;
  snapshot.outputs = {
      output(QStringLiteral("left"), QRect(-1920, 0, 1920, 1080)),
      output(QStringLiteral("right"), QRect(0, 0, 2560, 1440)),
  };
  snapshot.panels = {
      panel(Profiles::HideMode::Always, QStringLiteral("shared"),
            QStringLiteral("left"), QRect(-1920, 1040, 1920, 40)),
      panel(Profiles::HideMode::Always, QStringLiteral("shared"),
            QStringLiteral("right"), QRect(0, 1400, 2560, 40)),
  };
  snapshot.scope = {QStringLiteral("workspace-1"),
                    QStringLiteral("activity-a")};
  snapshot.interactions = {interaction(true, false, QStringLiteral("shared"),
                                       QStringLiteral("right"))};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions.size(), 2);
  QCOMPARE(result.decisions[0].identity.outputId, QStringLiteral("left"));
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[1].identity.outputId, QStringLiteral("right"));
  QCOMPARE(result.decisions[1].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[1].reason, PanelVisibilityReason::RevealRequested);
}

void PanelVisibilityScopeTest::
    validEmptyPanelSetProducesAnEmptyDecisionBatch() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels.clear();

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QVERIFY(result.decisions.isEmpty());
}

QTEST_GUILESS_MAIN(PanelVisibilityScopeTest)
#include "tst_panel_visibility_scope.moc"
