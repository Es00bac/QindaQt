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
  void activeOutputCoverWinsOverStaleRevealAndHold();
  void activeOutputCoverAffectsOnlyCoveredOutput();
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

void PanelVisibilityModesTest::activeOutputCoverWinsOverStaleRevealAndHold() {
  for (const auto mode : {Profiles::HideMode::Always,
                          Profiles::HideMode::DodgeActive,
                          Profiles::HideMode::DodgeAll,
                          Profiles::HideMode::Maximized,
                          Profiles::HideMode::Intelligent}) {
    auto snapshot = inventory(mode);
    snapshot.panels[0].surfaceGeometry = QRect(0, 1008, 1920, 72);
    auto fullscreen = window(QStringLiteral("fullscreen"),
                             QRect(0, 0, 1920, 1080));
    fullscreen.active = true;
    snapshot.windows = {fullscreen};
    snapshot.interactions = {interaction(true, true)};

    auto result = PanelVisibilityPolicy::evaluate(snapshot);
    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
    QCOMPARE(result.decisions[0].reservation, PanelReservationIntent::Release);
    QCOMPARE(result.decisions[0].reason,
             PanelVisibilityReason::ActiveWindowCoversOutput);
    QCOMPARE(result.decisions[0].triggerWindowId, QStringLiteral("fullscreen"));

    // An ordinary maximized client still permits an intentional edge reveal.
    snapshot.windows[0].frameGeometry = QRect(0, 32, 1920, 1048);
    snapshot.windows[0].maximized = true;
    result = PanelVisibilityPolicy::evaluate(snapshot);
    QVERIFY(result.ok());
    QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
    QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::VisibilityHeld);

    // An overlay-only layout may let an ordinary maximized window occupy
    // every output pixel. Its command strip must still be revealable.
    snapshot.windows[0].frameGeometry = QRect(0, 0, 1920, 1080);
    result = PanelVisibilityPolicy::evaluate(snapshot);
    QVERIFY(result.ok());
    QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
    QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::VisibilityHeld);

    // KWin may retain maximize state while the client is fullscreen.
    snapshot.windows[0].fullscreen = true;
    result = PanelVisibilityPolicy::evaluate(snapshot);
    QVERIFY(result.ok());
    QCOMPARE(result.decisions[0].visibility, PanelVisibility::Hidden);
    QCOMPARE(result.decisions[0].reason,
             PanelVisibilityReason::ActiveWindowCoversOutput);

    snapshot.windows[0].fullscreen = false;
    snapshot.windows[0].active = false;
    result = PanelVisibilityPolicy::evaluate(snapshot);
    QVERIFY(result.ok());
    QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  }

  auto never = inventory(Profiles::HideMode::Never);
  auto fullscreen = window(QStringLiteral("fullscreen"),
                           QRect(0, 0, 1920, 1080));
  fullscreen.active = true;
  never.windows = {fullscreen};
  const auto result = PanelVisibilityPolicy::evaluate(never);
  QVERIFY(result.ok());
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[0].reason, PanelVisibilityReason::NeverMode);
}

void PanelVisibilityModesTest::activeOutputCoverAffectsOnlyCoveredOutput() {
  auto snapshot = inventory(Profiles::HideMode::Intelligent);
  snapshot.outputs = {
      output(QStringLiteral("left"), QRect(-1600, 0, 1600, 900)),
      output(QStringLiteral("right"), QRect(0, 0, 1920, 1200))};
  snapshot.panels = {
      panel(Profiles::HideMode::Intelligent, QStringLiteral("left-dock"),
            QStringLiteral("left"), QRect(-1600, 840, 1600, 60)),
      panel(Profiles::HideMode::Intelligent, QStringLiteral("right-dock"),
            QStringLiteral("right"), QRect(0, 1140, 1920, 60))};
  auto fullscreen = window(QStringLiteral("fullscreen"),
                           QRect(0, 0, 1920, 1200), QStringLiteral("right"));
  fullscreen.active = true;
  snapshot.windows = {fullscreen};
  snapshot.interactions = {
      interaction(true, true, QStringLiteral("left-dock"),
                  QStringLiteral("left")),
      interaction(true, true, QStringLiteral("right-dock"),
                  QStringLiteral("right"))};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.decisions[0].visibility, PanelVisibility::Visible);
  QCOMPARE(result.decisions[1].visibility, PanelVisibility::Hidden);
  QCOMPARE(result.decisions[1].reason,
           PanelVisibilityReason::ActiveWindowCoversOutput);
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
