// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerplacement_fixture.h"
#include <limits>
using namespace QindaQt::Compositor::KWinIntegration;
using namespace PlacementFixtures;
class SemanticFractionTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void changingFractionAndAreaPreservesOriginalRestore() {
    Fixture f;
    QString error;
    const auto original = f.layout.outerFrame;
    f.workArea = {0, 40, 1920, 1040};
    QVERIFY(f.controller.maximizeFraction("group", 0.9, &error));
    QCOMPARE(f.layout.outerFrame, QRect(96, 92, 1728, 936));
    QVERIFY(f.controller.maximizeFraction("group", 0.8, &error));
    f.workArea = {-1280, 60, 1280, 960};
    QVERIFY(f.controller.refreshMaximizedAreas().isEmpty());
    QCOMPARE(f.layout.outerFrame, QRect(-1152, 156, 1024, 768));
    QVERIFY(f.controller.restore("group", &error));
    QCOMPARE(f.layout.outerFrame, original);
  }
  void failedFractionOrPlacementLeavesRestoreAndFractionIntact() {
    Fixture f;
    QString error;
    const auto original = f.layout.outerFrame;
    QVERIFY(f.controller.maximizeFraction("group", 0.9, &error));
    const auto applied = f.layout.outerFrame;
    f.failNext = true;
    QVERIFY(!f.controller.maximizeFraction("group", 0.5, &error));
    QCOMPARE(f.layout.outerFrame, applied);
    QVERIFY(f.controller.isMaximized("group"));
    f.failNext = true;
    QVERIFY(!f.controller.placeFrame("group", {30, 70, 800, 600}, &error));
    f.workArea.translate(50, 10);
    QVERIFY(f.controller.refreshMaximizedAreas().isEmpty());
    QCOMPARE(f.layout.outerFrame, applied.translated(50, 10));
    QVERIFY(f.controller.restore("group", &error));
    QCOMPARE(f.layout.outerFrame, original);
  }
  void shadeUnshadeAndResizeCancelKeepFraction() {
    Fixture f;
    QString error;
    QVERIFY(f.controller.maximizeFraction("group", 0.9, &error));
    QVERIFY(f.controller.shade("group", &error));
    f.workArea = {0, 50, 1280, 960};
    QVERIFY(f.controller.unshade("group", &error));
    QCOMPARE(f.layout.outerFrame, QRect(64, 98, 1152, 864));
    QVERIFY(f.controller
                .handleResize(
                    resizeIntent(QindaQt::HybridInput::IntentPhase::Begin))
                .accepted);
    QVERIFY(f.controller
                .handleResize(resizeIntent(
                    QindaQt::HybridInput::IntentPhase::Update, {20, 10}))
                .accepted);
    QVERIFY(f.controller
                .handleResize(
                    resizeIntent(QindaQt::HybridInput::IntentPhase::Cancel))
                .accepted);
    QVERIFY(f.controller.isMaximized("group"));
    QCOMPARE(f.layout.outerFrame, QRect(64, 98, 1152, 864));
  }
  void invalidFractionCannotChangeFrame() {
    Fixture f;
    QString error;
    const auto before = f.layout.outerFrame;
    QVERIFY(!f.controller.maximizeFraction(
        "group", std::numeric_limits<double>::quiet_NaN(), &error));
    QVERIFY(!f.controller.maximizeFraction("group", 0.05, &error));
    QCOMPARE(f.layout.outerFrame, before);
    QVERIFY(!f.controller.isMaximized("group"));
  }
};
QTEST_GUILESS_MAIN(SemanticFractionTest)
#include "tst_semanticfractionalmaximize.moc"
