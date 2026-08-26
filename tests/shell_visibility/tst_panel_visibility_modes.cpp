// SPDX-License-Identifier: GPL-3.0-or-later
#include "visibility_test_support.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::ShellVisibility;
using namespace QindaQt::ShellVisibility::TestSupport;

class PanelVisibilityModesTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void neverModeCannotBeHiddenByConflictsOrInteractionInputs();
  void alwaysModeRequiresAnExplicitRevealOrHold();
  void holdTakesDeterministicPriorityOverReveal();
  void dodgeActiveUsesOnlyTheActiveOverlappingWindow();
  void dodgeAllUsesAnyOverlappingWindow();
  void maximizedUsesFullMaximizeOnTheAssignedOutput();
  void intelligentCombinesActiveOverlapAndMaximizedOutput();
  void reservationIntentTracksVisibilityAndPanelPolicy();
};

void PanelVisibilityModesTest::
    neverModeCannotBeHiddenByConflictsOrInteractionInputs() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  auto conflict = window();
  conflict.active = true;
  conflict.maximized = true;
  snapshot.windows = {conflict};
  snapshot.interactions = {interaction(true, true)};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.decisions.size(), 1);
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reservation, PanelReservationIntent::Reserve);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::NeverMode);
  QVERIFY(result.decisions[0].triggerWindowId.isEmpty());
}

void PanelVisibilityModesTest::alwaysModeRequiresAnExplicitRevealOrHold() {
  auto snapshot = inventory(Profiles::HideMode::Always);

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[0].reservation, PanelReservationIntent::Release);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::AlwaysMode);

  snapshot.interactions = {interaction(true, false)};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::RevealRequested);

  snapshot.interactions = {interaction(false, true)};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::VisibilityHeld);
}

void PanelVisibilityModesTest::holdTakesDeterministicPriorityOverReveal() {
  auto snapshot = inventory(Profiles::HideMode::DodgeAll);
  snapshot.windows = {window()};
  snapshot.interactions = {interaction(true, true)};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::VisibilityHeld);
  QVERIFY(result.decisions[0].triggerWindowId.isEmpty());
}

void PanelVisibilityModesTest::dodgeActiveUsesOnlyTheActiveOverlappingWindow() {
  auto snapshot = inventory(Profiles::HideMode::DodgeActive);
  auto inactiveOverlap = window(QStringLiteral("inactive"));
  auto activeClear = window(QStringLiteral("active"), QRect(50, 200, 400, 300));
  activeClear.active = true;
  snapshot.windows = {inactiveOverlap, activeClear};

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::NoConflict);

  snapshot.windows[1].frameGeometry = QRect(100, 20, 400, 300);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[0].reason,
           PanelVisibilityReason::ActiveWindowOverlap);
  QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("active"));
}

void PanelVisibilityModesTest::dodgeAllUsesAnyOverlappingWindow() {
  auto snapshot = inventory(Profiles::HideMode::DodgeAll);
  snapshot.windows = {
      window(QStringLiteral("first")),
      window(QStringLiteral("second"), QRect(100, 10, 500, 300))};

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::AnyWindowOverlap);
  QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("first"));

  snapshot.windows[0].frameGeometry = QRect(0, 100, 200, 200);
  snapshot.windows[1].frameGeometry = QRect(300, 100, 200, 200);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::NoConflict);
}

void PanelVisibilityModesTest::maximizedUsesFullMaximizeOnTheAssignedOutput() {
  auto snapshot = inventory(Profiles::HideMode::Maximized);
  snapshot.outputs.push_back(
      output(QStringLiteral("left"), QRect(-1600, 0, 1600, 900)));
  auto otherOutput =
      window(QStringLiteral("other"), QRect(-1500, 100, 1200, 700),
             QStringLiteral("left"));
  otherOutput.maximized = true;
  snapshot.windows = {otherOutput};

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);

  auto local = window(QStringLiteral("local"), QRect(100, 200, 700, 500));
  local.maximized = true;
  snapshot.windows.push_back(local);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[0].reason,
           PanelVisibilityReason::MaximizedWindowOnOutput);
  QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("local"));
}

void PanelVisibilityModesTest::
    intelligentCombinesActiveOverlapAndMaximizedOutput() {
  auto snapshot = inventory(Profiles::HideMode::Intelligent);
  auto maximized =
      window(QStringLiteral("maximized"), QRect(100, 200, 700, 500));
  maximized.maximized = true;
  auto active = window(QStringLiteral("active"), QRect(20, 10, 600, 400));
  active.active = true;
  snapshot.windows = {maximized, active};

  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].reason,
           PanelVisibilityReason::ActiveWindowOverlap);
  QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("active"));

  snapshot.windows[1].frameGeometry = QRect(20, 100, 600, 400);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].reason,
           PanelVisibilityReason::MaximizedWindowOnOutput);
  QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("maximized"));

  snapshot.windows[0].maximized = false;
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::NoConflict);
}

void PanelVisibilityModesTest::
    reservationIntentTracksVisibilityAndPanelPolicy() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels[0].reservationPolicy = PanelReservationPolicy::NeverReserve;
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reservation, PanelReservationIntent::Release);

  snapshot.panels[0].hideMode = Profiles::HideMode::Always;
  snapshot.panels[0].reservationPolicy =
      PanelReservationPolicy::ReserveWhenVisible;
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].reservation, PanelReservationIntent::Release);

  snapshot.interactions = {interaction(true, false)};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].reservation, PanelReservationIntent::Reserve);
}

QTEST_GUILESS_MAIN(PanelVisibilityModesTest)
#include "tst_panel_visibility_modes.moc"
