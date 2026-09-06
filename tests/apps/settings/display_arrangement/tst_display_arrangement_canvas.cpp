// SPDX-License-Identifier: GPL-3.0-or-later

// Arrangement canvas behavior against the real DisplaySettingsModel: logical
// geometry and numbering, pointer drag with snapping, keyboard nudges, quick
// placement, revert, and the connected-but-disabled case.

#include "arrangement_test_support.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QSignalSpy>
#include <QTest>
#include <cmath>

namespace Support = QindaQt::Tests::DisplayArrangementSupport;
using Support::findItemByObjectName;

namespace {

const QString kMain = QStringLiteral("edid:dp1");
const QString kSide = QStringLiteral("edid:hdmi1");

double canvasFit(QQuickItem *canvas) {
  return canvas->property("layout").toMap().value(QStringLiteral("fit")).toDouble();
}

} // namespace

class DisplayArrangementCanvasTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testTilesShowLogicalGeometryWithMatchingNumbers();
  void testDragSnapsLeftIntoNegativeCoordinatesAndAlignsTop();
  void testDragSlidesAlongEdgeBeyondAlignmentThreshold();
  void testKeyboardNudgesSlideAlongAttachedEdge();
  void testQuickPlacementCoversCommonCasesAndRevertRestores();
  void testDisabledOutputIsSelectableButNotDraggable();

private:
  std::unique_ptr<QQuickView> m_view;
};

void DisplayArrangementCanvasTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  QString error;
  QVERIFY2(Support::bootstrapTokens(*m_view, &error), qPrintable(error));
}

void DisplayArrangementCanvasTest::testTilesShowLogicalGeometryWithMatchingNumbers() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));

  auto *canvas = findItemByObjectName(page.page, QStringLiteral("displayArrangementCanvas"));
  QVERIFY(canvas != nullptr);
  QTRY_COMPARE(canvas->property("tileCount").toInt(), 2);
  auto *mainTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kMain);
  auto *sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  QVERIFY(mainTile != nullptr);
  QVERIFY(sideTile != nullptr);

  // Both displays are 1920x1080 logical (4K at 200%, 1080p at 100%), so the
  // tiles share a size and the side tile abuts the main tile's right edge.
  QVERIFY(mainTile->width() > 40);
  QCOMPARE(std::lround(sideTile->width()), std::lround(mainTile->width()));
  QCOMPARE(std::lround(sideTile->height()), std::lround(mainTile->height()));
  QCOMPARE(std::lround(sideTile->x()), std::lround(mainTile->x() + mainTile->width()));
  QCOMPARE(std::lround(sideTile->y()), std::lround(mainTile->y()));
  QCOMPARE(mainTile->property("logicalWidth").toInt(), 1920);
  QCOMPARE(mainTile->property("logicalHeight").toInt(), 1080);
  QCOMPARE(sideTile->property("logicalX").toInt(), 1920);

  // The cards retain native resolution separately from this logical diagram.
  const auto cards = Support::itemsWithProperty(page.page, "outputData");
  QCOMPARE(cards.size(), 2);
  for (auto *card : cards) {
    const auto stableId = card->property("outputData").toMap().value(QStringLiteral("stableId")).toString();
    if (stableId == kMain) {
      QCOMPARE(card->property("summaryText").toString(),
               QStringLiteral("3840 × 2160 pixels · 200% scale"));
    }
  }

  // Numbering matches the inventory cards.
  QCOMPARE(mainTile->property("ordinal").toInt(), 1);
  QCOMPARE(sideTile->property("ordinal").toInt(), 2);
  for (auto *card : cards) {
    const auto stableId = card->property("outputData").toMap().value(QStringLiteral("stableId")).toString();
    QCOMPARE(card->property("ordinal").toInt(), stableId == kMain ? 1 : 2);
  }

  auto *readout = findItemByObjectName(page.page, QStringLiteral("displayArrangementReadout"));
  QVERIFY(readout != nullptr);
  QVERIFY(readout->property("text").toString().contains(QStringLiteral("1 Main Monitor at 0, 0")));
  QVERIFY(readout->property("text").toString().contains(QStringLiteral("1920 × 1080")));

  auto *accessible = QAccessible::queryAccessibleInterface(sideTile);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::Button);
  QVERIFY(accessible->text(QAccessible::Name).contains(QStringLiteral("Display 2")));
  QVERIFY(accessible->text(QAccessible::Description).contains(QStringLiteral("1920, 0")));

  Support::keepRender(*m_view, QStringLiteral("arrangement-two-monitors"));
}

void DisplayArrangementCanvasTest::testDragSnapsLeftIntoNegativeCoordinatesAndAlignsTop() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));
  auto *canvas = findItemByObjectName(page.page, QStringLiteral("displayArrangementCanvas"));
  QVERIFY(canvas != nullptr);
  QTRY_COMPARE(canvas->property("tileCount").toInt(), 2);
  auto *sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  auto *mainTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kMain);
  QVERIFY(sideTile != nullptr);
  QVERIFY(mainTile != nullptr);

  // Pressing selects; the model is untouched until release.
  QCOMPARE(rig.model.selectedOutputId(), kMain);
  const int tileWidth = int(std::lround(mainTile->width()));
  Support::dragBy(*m_view, sideTile, QPoint(-tileWidth * 2, 5));
  QCOMPARE(rig.model.selectedOutputId(), kSide);

  // Dragged past the main display it snaps to its left edge (negative x) and
  // the 5px wobble is pulled back onto the shared top edge.
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(-1920, 0));
  QVERIFY(rig.model.draftDirty());
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.errorText().isEmpty());
  QVERIFY(rig.model.warnings().isEmpty());

  // The diagram, readout, and precision fields all follow the draft.
  sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  mainTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kMain);
  QVERIFY(sideTile != nullptr);
  QVERIFY(mainTile != nullptr);
  QTRY_COMPARE(std::lround(mainTile->x()), std::lround(sideTile->x() + sideTile->width()));
  auto *posX = findItemByObjectName(page.page, QStringLiteral("displayPosXField"));
  QVERIFY(posX != nullptr);
  QTRY_COMPARE(posX->property("text").toString(), QStringLiteral("-1920"));
  auto *readout = findItemByObjectName(page.page, QStringLiteral("displayArrangementReadout"));
  QVERIFY(readout != nullptr);
  QVERIFY(readout->property("text").toString().contains(QStringLiteral("at -1920, 0")));

  Support::keepRender(*m_view, QStringLiteral("arrangement-side-left"));
}

void DisplayArrangementCanvasTest::testDragSlidesAlongEdgeBeyondAlignmentThreshold() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));
  auto *canvas = findItemByObjectName(page.page, QStringLiteral("displayArrangementCanvas"));
  QVERIFY(canvas != nullptr);
  QTRY_COMPARE(canvas->property("tileCount").toInt(), 2);
  auto *sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  QVERIFY(sideTile != nullptr);

  const double fit = canvasFit(canvas);
  QVERIFY(fit > 0.0);
  const int screenDelta = 60;
  Support::dragBy(*m_view, sideTile, QPoint(0, screenDelta));

  // Far outside the 16px alignment threshold the display slides down the
  // shared edge and stays attached; the draft stays valid and gap-free.
  const int expectedY = int(std::lround(screenDelta / fit));
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, expectedY));
  QVERIFY(expectedY > 200);
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.warnings().isEmpty());

  // Dropping it over the main display cannot overlap: it lands on the nearest
  // free edge instead.
  sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  QVERIFY(sideTile != nullptr);
  auto *mainTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kMain);
  QVERIFY(mainTile != nullptr);
  const QPoint toMainCenter = Support::sceneCenter(mainTile) - Support::sceneCenter(sideTile);
  Support::dragBy(*m_view, sideTile, toMainCenter + QPoint(0, int(mainTile->height() / 3)));
  QTRY_VERIFY(rig.model.draftValid());
  const QPoint dropped = rig.positionOf(kSide);
  QVERIFY(dropped.y() >= 1080 || dropped.x() >= 1920 || dropped.x() <= -1920);
  QVERIFY(rig.model.warnings().isEmpty());
}

void DisplayArrangementCanvasTest::testKeyboardNudgesSlideAlongAttachedEdge() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));
  auto *canvas = findItemByObjectName(page.page, QStringLiteral("displayArrangementCanvas"));
  QVERIFY(canvas != nullptr);
  QTRY_COMPARE(canvas->property("tileCount").toInt(), 2);
  auto *sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  QVERIFY(sideTile != nullptr);
  QVERIFY(sideTile->activeFocusOnTab());

  sideTile->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), sideTile);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QCOMPARE(rig.model.selectedOutputId(), kSide);

  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 10));
  // The tile survives the draft edit and keeps focus for the next key.
  QCOMPARE(m_view->activeFocusItem(), sideTile);
  QTest::keyClick(m_view.get(), Qt::Key_Down, Qt::ShiftModifier);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 110));
  QTest::keyClick(m_view.get(), Qt::Key_Up);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 100));

  // Nudging into the neighbour keeps the display attached instead of
  // overlapping; nudging away keeps it attached instead of opening a gap.
  QTest::keyClick(m_view.get(), Qt::Key_Left);
  QCoreApplication::processEvents();
  QCOMPARE(rig.positionOf(kSide), QPoint(1920, 100));
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QCoreApplication::processEvents();
  QCOMPARE(rig.positionOf(kSide), QPoint(1920, 100));
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.warnings().isEmpty());

  // Revert restores the snapshot position underneath the focused tile.
  rig.model.cancelDraft();
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 0));
}

void DisplayArrangementCanvasTest::testQuickPlacementCoversCommonCasesAndRevertRestores() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));
  rig.model.setSelectedOutputId(kSide);

  auto *referenceLabel = findItemByObjectName(page.page, QStringLiteral("displayPlacementReferenceLabel"));
  QVERIFY(referenceLabel != nullptr);
  QTRY_VERIFY(referenceLabel->property("text").toString().contains(QStringLiteral("1 Main Monitor")));

  const struct {
    const char *button;
    QPoint expected;
  } cases[] = {
      {"displayPlaceAboveButton", QPoint(0, -1080)},
      {"displayPlaceRightButton", QPoint(1920, 0)},
      {"displayPlaceBelowButton", QPoint(0, 1080)},
      {"displayPlaceLeftButton", QPoint(-1920, 0)},
  };
  for (const auto &placement : cases) {
    auto *button = findItemByObjectName(page.page, QString::fromLatin1(placement.button));
    QVERIFY2(button != nullptr, placement.button);
    QVERIFY(button->property("available").toBool());
    QMetaObject::invokeMethod(button, "clicked");
    QTRY_COMPARE(rig.positionOf(kSide), placement.expected);
    QVERIFY(rig.model.draftValid());
    QVERIFY(rig.model.warnings().isEmpty());
  }

  // Revert restores the snapshot arrangement and the diagram follows.
  auto *revert = findItemByObjectName(page.page, QStringLiteral("displayRevertButton"));
  QVERIFY(revert != nullptr);
  QMetaObject::invokeMethod(revert, "clicked");
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 0));
  QVERIFY(!rig.model.draftDirty());
  auto *sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  auto *mainTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kMain);
  QVERIFY(sideTile != nullptr);
  QVERIFY(mainTile != nullptr);
  QTRY_COMPARE(std::lround(sideTile->x()), std::lround(mainTile->x() + mainTile->width()));
}

void DisplayArrangementCanvasTest::testDisabledOutputIsSelectableButNotDraggable() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::connectedDisabledSideSnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));
  auto *canvas = findItemByObjectName(page.page, QStringLiteral("displayArrangementCanvas"));
  QVERIFY(canvas != nullptr);
  QTRY_COMPARE(canvas->property("tileCount").toInt(), 1);
  QVERIFY(findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide) == nullptr);

  // A lone enabled display has no relative placement. Dragging or nudging it
  // must remain visibly inert, matching topology's required origin.
  auto *mainTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kMain);
  QVERIFY(mainTile != nullptr);
  Support::dragBy(*m_view, mainTile, QPoint(120, 80));
  QCOMPARE(rig.positionOf(kMain), QPoint(0, 0));
  mainTile->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), mainTile);
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QCOMPARE(rig.positionOf(kMain), QPoint(0, 0));
  QVERIFY(!rig.model.draftDirty());
  auto *apply = findItemByObjectName(page.page, QStringLiteral("displayApplyButton"));
  QVERIFY(apply != nullptr);
  QVERIFY(!apply->property("available").toBool());
  auto *singleReadout = findItemByObjectName(page.page, QStringLiteral("displayArrangementReadout"));
  QVERIFY(singleReadout != nullptr);
  QVERIFY(singleReadout->property("text").toString().contains(QStringLiteral("at 0, 0")));

  auto *inactiveTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementInactiveTile_") + kSide);
  QVERIFY(inactiveTile != nullptr);
  QVERIFY(inactiveTile->isVisible());
  QCOMPARE(inactiveTile->property("ordinal").toInt(), 2);
  QVERIFY(!inactiveTile->property("draggable").toBool());

  auto *enableRow = findItemByObjectName(page.page, QStringLiteral("displayEnableFormRow"));
  QVERIFY(enableRow != nullptr);
  QCOMPARE(enableRow->property("description").toString(),
           QStringLiteral("This display is ready to configure."));

  QCOMPARE(rig.model.selectedOutputId(), kMain);
  Support::dragBy(*m_view, inactiveTile, QPoint(-120, 0));
  QCOMPARE(rig.model.selectedOutputId(), kSide);
  QCOMPARE(enableRow->property("description").toString(),
           QStringLiteral("Turn on this connected display before configuring it."));
  QVERIFY(!rig.model.draftDirty());
  QCOMPARE(rig.positionOf(kSide), QPoint(0, 0));

  auto *readout = findItemByObjectName(page.page, QStringLiteral("displayArrangementReadout"));
  QVERIFY(readout != nullptr);
  QTRY_VERIFY(readout->property("text").toString().contains(QStringLiteral("Side Monitor is off")));
  auto *leftButton = findItemByObjectName(page.page, QStringLiteral("displayPlaceLeftButton"));
  QVERIFY(leftButton != nullptr);
  QVERIFY(!leftButton->property("available").toBool());

  // Enabling it through the existing switch places it in the diagram.
  auto *enableSwitch = findItemByObjectName(page.page, QStringLiteral("displayEnableSwitch"));
  QVERIFY(enableSwitch != nullptr);
  QMetaObject::invokeMethod(enableSwitch, "toggle");
  QMetaObject::invokeMethod(enableSwitch, "toggled");
  QTRY_COMPARE(canvas->property("tileCount").toInt(), 2);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 0));
  QVERIFY(rig.model.draftValid());
  QTRY_VERIFY(leftButton->property("available").toBool());
  Support::keepRender(*m_view, QStringLiteral("arrangement-enabled-side"));
}

QTEST_MAIN(DisplayArrangementCanvasTest)
#include "tst_display_arrangement_canvas.moc"
