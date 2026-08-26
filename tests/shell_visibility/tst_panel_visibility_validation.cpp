// SPDX-License-Identifier: GPL-3.0-or-later
#include "visibility_test_support.h"

#include <QTest>

#include <limits>

using namespace QindaQt;
using namespace QindaQt::ShellVisibility;
using namespace QindaQt::ShellVisibility::TestSupport;

class PanelVisibilityValidationTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rejectsInvalidScopeAndEmptyOutputInventoryDeterministically();
  void rejectsInvalidDuplicateAndUnsafeOutputs();
  void rejectsPanelIdentityAndOutputFailuresAtomically();
  void rejectsPanelGeometryAndEnumFailuresAtomically();
  void rejectsWindowIdentityOutputAndGeometryFailures();
  void rejectsMalformedWindowScopeAndState();
  void rejectsInvalidDuplicateAndUnknownInteractions();
  void rejectsMalformedUtf16Identifiers();
  void acceptsOverlappingOutputsAndNegativeCoordinates();
};

void PanelVisibilityValidationTest::
    rejectsInvalidScopeAndEmptyOutputInventoryDeterministically() {
  PanelVisibilityInventory snapshot;
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidScope);
  QVERIFY(result.decisions.isEmpty());

  snapshot.scope = {QStringLiteral("workspace-1"),
                    QStringLiteral("activity-a")};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY(!result.ok());
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::EmptyOutputInventory);
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::rejectsInvalidDuplicateAndUnsafeOutputs() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.outputs[0].id = QStringLiteral(" eDP-1");
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidOutputId);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.outputs.push_back(output());
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::DuplicateOutputId);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.outputs[0].geometry = QRect();
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidOutputGeometry);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.outputs[0].geometry =
      QRect(QPoint(std::numeric_limits<int>::min(), 0),
            QPoint(std::numeric_limits<int>::max(), 100));
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidOutputGeometry);
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::
    rejectsPanelIdentityAndOutputFailuresAtomically() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels.push_back(panel(Profiles::HideMode::Always,
                                  QStringLiteral(" "),
                                  QStringLiteral("eDP-1")));
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidPanelIdentity);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels.push_back(panel(Profiles::HideMode::Always));
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::DuplicatePanelIdentity);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels.push_back(panel(Profiles::HideMode::Always,
                                  QStringLiteral("missing-panel"),
                                  QStringLiteral("missing")));
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::UnknownPanelOutput);
  QCOMPARE(result.error.outputId, QStringLiteral("missing"));
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::
    rejectsPanelGeometryAndEnumFailuresAtomically() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels.push_back(panel(Profiles::HideMode::Always,
                                  QStringLiteral("invalid-geometry"),
                                  QStringLiteral("eDP-1"), QRect()));
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidPanelGeometry);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels.push_back(
      panel(Profiles::HideMode::Always, QStringLiteral("outside"),
            QStringLiteral("eDP-1"), QRect(-1, 0, 1920, 32)));
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::PanelOutsideOutput);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels[0].hideMode = static_cast<Profiles::HideMode>(99);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidHideMode);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.panels[0].reservationPolicy =
      static_cast<PanelReservationPolicy>(99);
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code,
           PanelVisibilityErrorCode::InvalidReservationPolicy);
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::
    rejectsWindowIdentityOutputAndGeometryFailures() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {window(QStringLiteral(" window"))};
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidWindowId);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {window(), window()};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::DuplicateWindowId);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {window(QStringLiteral("window"), QRect(0, 0, 100, 100),
                             QStringLiteral("missing"))};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::UnknownWindowOutput);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {window()};
  snapshot.windows[0].frameGeometry = QRect();
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidWindowGeometry);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {
      window(QStringLiteral("offscreen"), QRect(3000, 3000, 100, 100))};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code,
           PanelVisibilityErrorCode::WindowOutsideAssignedOutput);
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::rejectsMalformedWindowScopeAndState() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {window()};
  snapshot.windows[0].workspaceId.clear();
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidWindowScope);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {window()};
  snapshot.windows[0].onAllWorkspaces = true;
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidWindowScope);

  snapshot.windows[0].workspaceId.clear();
  snapshot.windows[0].activityIds = {QStringLiteral("activity-a"),
                                     QStringLiteral("activity-a")};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidWindowScope);

  snapshot = inventory(Profiles::HideMode::Never);
  snapshot.windows = {window()};
  snapshot.windows[0].active = true;
  snapshot.windows[0].minimized = true;
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidWindowState);

  snapshot = inventory(Profiles::HideMode::Never);
  auto first = window(QStringLiteral("first"));
  auto second = window(QStringLiteral("second"));
  first.active = true;
  second.active = true;
  snapshot.windows = {first, second};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::MultipleActiveWindows);
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::
    rejectsInvalidDuplicateAndUnknownInteractions() {
  auto snapshot = inventory(Profiles::HideMode::Always);
  snapshot.interactions = {interaction(true, false, QStringLiteral(" panel"))};
  auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code,
           PanelVisibilityErrorCode::InvalidInteractionIdentity);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Always);
  snapshot.interactions = {interaction(true, false, QStringLiteral("missing"))};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code,
           PanelVisibilityErrorCode::UnknownInteractionPanel);
  QVERIFY(result.decisions.isEmpty());

  snapshot = inventory(Profiles::HideMode::Always);
  snapshot.interactions = {interaction(true, false), interaction(false, true)};
  result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code,
           PanelVisibilityErrorCode::DuplicateInteractionIdentity);
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::rejectsMalformedUtf16Identifiers() {
  auto snapshot = inventory(Profiles::HideMode::Never);
  snapshot.outputs[0].id = QString(1, QChar(0xd800));
  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QCOMPARE(result.error.code, PanelVisibilityErrorCode::InvalidOutputId);
  QVERIFY(result.decisions.isEmpty());
}

void PanelVisibilityValidationTest::
    acceptsOverlappingOutputsAndNegativeCoordinates() {
  PanelVisibilityInventory snapshot;
  snapshot.outputs = {
      output(QStringLiteral("mirror-a"), QRect(-1920, -200, 1920, 1080)),
      output(QStringLiteral("mirror-b"), QRect(-1920, -200, 1920, 1080)),
  };
  snapshot.panels = {
      panel(Profiles::HideMode::Never, QStringLiteral("a"),
            QStringLiteral("mirror-a"), QRect(-1920, -200, 1920, 32)),
      panel(Profiles::HideMode::Never, QStringLiteral("b"),
            QStringLiteral("mirror-b"), QRect(-1920, 848, 1920, 32)),
  };
  snapshot.scope = {QStringLiteral("workspace-1"),
                    QStringLiteral("activity-a")};

  const auto result = PanelVisibilityPolicy::evaluate(snapshot);
  QVERIFY2(result.ok(), qPrintable(result.error.message));
  QCOMPARE(result.decisions.size(), 2);
  QCOMPARE(result.decisions[0].identity.panelId, QStringLiteral("a"));
  QCOMPARE(result.decisions[1].identity.panelId, QStringLiteral("b"));
}

QTEST_GUILESS_MAIN(PanelVisibilityValidationTest)
#include "tst_panel_visibility_validation.moc"
