// SPDX-License-Identifier: GPL-3.0-or-later

// Scale guidance and the arrangement's reaction to size changes against the
// real DisplaySettingsModel: neighbours re-attach after a scale change with
// no drift, untouched sides stay put, rows propagate, and reverts never
// trigger a repair.

#include "arrangement_test_support.h"

#include <QTest>

namespace Support = QindaQt::Tests::DisplayArrangementSupport;
using Support::findItemByObjectName;

namespace {

const QString kMain = QStringLiteral("edid:dp1");
const QString kSide = QStringLiteral("edid:hdmi1");
const QString kFar = QStringLiteral("edid:dp2");

void clickScale(QQuickItem *page, int percent) {
  auto *button = findItemByObjectName(
      page, QStringLiteral("displayScaleButton_%1").arg(percent));
  QVERIFY(button != nullptr);
  QVERIFY(button->property("available").toBool());
  QMetaObject::invokeMethod(button, "clicked");
}

} // namespace

class DisplayArrangementScaleTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testGuidanceDescribesSelectedDisplayPresets();
  void testNeighbourReattachesAfterScaleChangeWithoutDrift();
  void testLeftNeighbourAndRotationKeepValidArrangement();
  void testRowOfThreePropagatesAlongTheRow();
  void testRevertAndSnapshotResetNeverRepair();

private:
  std::unique_ptr<QQuickView> m_view;
};

void DisplayArrangementScaleTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  QString error;
  QVERIFY2(Support::bootstrapTokens(*m_view, &error), qPrintable(error));
}

void DisplayArrangementScaleTest::testGuidanceDescribesSelectedDisplayPresets() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));

  auto *guidance = findItemByObjectName(page.page, QStringLiteral("displayScaleGuidance"));
  QVERIFY(guidance != nullptr);
  const QString text = guidance->property("text").toString();
  QVERIFY2(text.contains(QStringLiteral("3840 × 2160")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("200%")), qPrintable(text));

  auto *scaleGrid = findItemByObjectName(page.page, QStringLiteral("displayScaleChoiceRow"));
  QVERIFY(scaleGrid != nullptr);
  QCOMPARE(scaleGrid->property("columns").toInt(), 4);
  auto *whole = findItemByObjectName(page.page, QStringLiteral("displayScaleButton_200"));
  auto *fractional = findItemByObjectName(page.page, QStringLiteral("displayScaleButton_175"));
  QVERIFY(whole != nullptr);
  QVERIFY(fractional != nullptr);
  QVERIFY(whole->property("checked").toBool());
  const QString wholeDescription = whole->property("accessibleDescription").toString();
  QVERIFY2(wholeDescription.contains(QStringLiteral("1920 × 1080")), qPrintable(wholeDescription));
  QVERIFY2(wholeDescription.contains(QStringLiteral("Typical")), qPrintable(wholeDescription));
  const QString fractionalDescription = fractional->property("accessibleDescription").toString();
  QVERIFY2(fractionalDescription.contains(QStringLiteral("fractional")), qPrintable(fractionalDescription));
  QVERIFY2(fractionalDescription.contains(QStringLiteral("2194 × 1234")), qPrintable(fractionalDescription));

  // Guidance follows the selection: the 1080p side display is typical at 100%.
  rig.model.setSelectedOutputId(kSide);
  QTRY_VERIFY(guidance->property("text").toString().contains(QStringLiteral("1920 × 1080: 100%")));
}

void DisplayArrangementScaleTest::testNeighbourReattachesAfterScaleChangeWithoutDrift() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));

  // 4K at 100% is 3840 wide: the side display would overlap at x=1920, so it
  // re-attaches at the new right edge and the draft stays admissible.
  clickScale(page.page, 100);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(3840, 0));
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.errorText().isEmpty());
  QVERIFY(rig.model.warnings().isEmpty());
  QCOMPARE(rig.output(kMain).value(QStringLiteral("logicalWidth")).toInt(), 3840);

  auto *sideTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kSide);
  auto *mainTile = findItemByObjectName(page.page, QStringLiteral("displayArrangementTile_") + kMain);
  QVERIFY(sideTile != nullptr);
  QVERIFY(mainTile != nullptr);
  QTRY_COMPARE(std::lround(sideTile->x()), std::lround(mainTile->x() + mainTile->width()));
  QTRY_COMPARE(std::lround(mainTile->width()), std::lround(sideTile->width() * 2));
  Support::keepRender(*m_view, QStringLiteral("arrangement-main-at-100"));

  // 150% shrinks it to 2560 wide: the neighbour follows the edge back in.
  clickScale(page.page, 150);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(2560, 0));
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.warnings().isEmpty());

  // Back to 200%: the original arrangement, no accumulated drift.
  clickScale(page.page, 200);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 0));
  QCOMPARE(rig.positionOf(kMain), QPoint(0, 0));
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.warnings().isEmpty());
  QVERIFY(!rig.model.draftDirty());
}

void DisplayArrangementScaleTest::testLeftNeighbourAndRotationKeepValidArrangement() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));

  // A neighbour on the left is not disturbed by growth to the right.
  QVERIFY(rig.model.setOutputPosition(kSide, -1920, 0));
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(-1920, 0));
  clickScale(page.page, 100);
  QTRY_COMPARE(rig.output(kMain).value(QStringLiteral("logicalWidth")).toInt(), 3840);
  QCoreApplication::processEvents();
  QCOMPARE(rig.positionOf(kSide), QPoint(-1920, 0));
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.warnings().isEmpty());

  // Rotating the side display (1080 wide, 1920 tall) keeps it attached on the
  // left: the section reacts to any in-place size change, not only scale.
  QVERIFY(rig.model.setOutputTransform(kSide, QStringLiteral("90")));
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(-1080, 0));
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.warnings().isEmpty());
}

void DisplayArrangementScaleTest::testRowOfThreePropagatesAlongTheRow() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::threeInARowSnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));
  auto *canvas = findItemByObjectName(page.page, QStringLiteral("displayArrangementCanvas"));
  QVERIFY(canvas != nullptr);
  QTRY_COMPARE(canvas->property("tileCount").toInt(), 3);

  clickScale(page.page, 100);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(3840, 0));
  QTRY_COMPARE(rig.positionOf(kFar), QPoint(5760, 0));
  QVERIFY(rig.model.draftValid());
  QVERIFY(rig.model.warnings().isEmpty());

  clickScale(page.page, 200);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 0));
  QTRY_COMPARE(rig.positionOf(kFar), QPoint(3840, 0));
  QVERIFY(!rig.model.draftDirty());

  // With two other displays the quick placement offers a reference selector.
  rig.model.setSelectedOutputId(kFar);
  auto *selector = findItemByObjectName(page.page, QStringLiteral("displayPlacementReferenceSelector"));
  QVERIFY(selector != nullptr);
  QTRY_COMPARE(selector->property("count").toInt(), 2);
  auto *leftButton = findItemByObjectName(page.page, QStringLiteral("displayPlaceLeftButton"));
  QVERIFY(leftButton != nullptr);
  QMetaObject::invokeMethod(leftButton, "clicked");
  QTRY_COMPARE(rig.positionOf(kFar), QPoint(-1920, 0));
  QVERIFY(rig.model.draftValid());
  Support::keepRender(*m_view, QStringLiteral("arrangement-three-monitors"));
}

void DisplayArrangementScaleTest::testRevertAndSnapshotResetNeverRepair() {
  Support::ModelRig rig;
  QVERIFY(rig.publish(Support::mixedDensitySnapshot()));
  QString error;
  auto page = Support::loadPage(*m_view, &rig.model, &error);
  QVERIFY2(page.page != nullptr, qPrintable(error));

  clickScale(page.page, 100);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(3840, 0));
  QVERIFY(rig.model.cancelDraft());
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(1920, 0));
  QCoreApplication::processEvents();
  QVERIFY(!rig.model.draftDirty());
  QCOMPARE(rig.positionOf(kSide), QPoint(1920, 0));

  // A fresh service snapshot with a deliberate gap is applied truth, not a
  // draft: the section must not "repair" it.
  auto gapped = Support::mixedDensitySnapshot();
  gapped.revision = 2;
  gapped.outputs[0].scale = 1.0;
  gapped.outputs[0].logicalSize = QSize(3840, 2160);
  gapped.outputs[1].position = QPoint(4000, 0);
  rig.transport.publishInvalidation(QStringLiteral(":1.50"), QStringLiteral("ep1"), 2);
  QTRY_VERIFY(rig.transport.fetches.size() >= 2);
  rig.transport.replySnapshot(rig.transport.fetches.last(), gapped);
  QTRY_COMPARE(rig.positionOf(kSide), QPoint(4000, 0));
  QCoreApplication::processEvents();
  QCoreApplication::processEvents();
  QCOMPARE(rig.positionOf(kSide), QPoint(4000, 0));
  QVERIFY(!rig.model.draftDirty());
}

QTEST_MAIN(DisplayArrangementScaleTest)
#include "tst_display_arrangement_scale.moc"
