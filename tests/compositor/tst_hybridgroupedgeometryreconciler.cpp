// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridgroupedgeometryreconciler.h"

#include <QtTest>
#include <QSet>

#include <algorithm>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

class Fixture final
{
public:
    Fixture()
        : reconciler(
              [this] { return windows; },
              [this](const QString &windowId, const QRectF &frame, QString *error) {
                  applied.append(windowId);
                  auto found = std::find_if(
                      windows.begin(), windows.end(),
                      [&windowId](const auto &window) {
                          return window.windowId == windowId;
                      });
                  if (found == windows.end() || rejected.contains(windowId)) {
                      if (error) {
                          *error = QStringLiteral("apply sentinel");
                      }
                      return false;
                  }
                  found->requestedFrame = frame;
                  return true;
              })
    {
    }

    QVector<GroupedWindowGeometry> windows;
    QStringList applied;
    QSet<QString> rejected;
    HybridGroupedGeometryReconciler reconciler;
};

} // namespace

class HybridGroupedGeometryReconcilerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void restoresOnlyDivergedOwnedMembersAfterDockReveal();
    void followsUpdatedMaximizedTargetsAcrossWorkAreaChanges();
    void preservesTemporaryNativePresentation();
    void reportsInvalidAndRejectedMembersWithoutStoppingPeers();
};

void HybridGroupedGeometryReconcilerTest::
    restoresOnlyDivergedOwnedMembersAfterDockReveal()
{
    Fixture fixture;
    const QRectF editorTarget(500, 394, 479, 675);
    const QRectF settingsTarget(19, 394, 479, 675);
    fixture.windows = {
        {.windowId = QStringLiteral("settings"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = settingsTarget,
         .targetFrame = settingsTarget},
        {.windowId = QStringLiteral("editor"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = QRectF(500, 333, 479, 675),
         .targetFrame = editorTarget},
        {.windowId = QStringLiteral("ordinary"),
         .containerId = {},
         .requestedFrame = QRectF(0, 0, 640, 480),
         .targetFrame = QRectF(20, 20, 640, 480)},
    };

    QVERIFY(fixture.reconciler.reconcile().isEmpty());
    QCOMPARE(fixture.applied, QStringList{QStringLiteral("editor")});
    QCOMPARE(fixture.windows[1].requestedFrame, editorTarget);

    fixture.applied.clear();
    QVERIFY(fixture.reconciler.reconcile().isEmpty());
    QVERIFY(fixture.applied.isEmpty());
}

void HybridGroupedGeometryReconcilerTest::preservesTemporaryNativePresentation()
{
    Fixture fixture;
    fixture.windows = {
        {.windowId = QStringLiteral("focused-member"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = QRectF(0, 0, 1920, 1080),
         .targetFrame = QRectF(500, 394, 479, 675),
         .nativeFrameOverride = true},
        {.windowId = QStringLiteral("minimized-member"),
         .containerId = QStringLiteral("minimized-group"),
         .requestedFrame = QRectF(0, 0, 100, 100),
         .targetFrame = QRectF(19, 394, 479, 675),
         .nativeFrameOverride = true},
        {.windowId = QStringLiteral("whole-group-maximized-member"),
         .containerId = QStringLiteral("maximized-group"),
         .requestedFrame = QRectF(0, 30, 960, 978),
         .targetFrame = QRectF(0, 30, 960, 906),
         .nativeFrameOverride = true},
    };

    QVERIFY(fixture.reconciler.reconcile().isEmpty());
    QVERIFY(fixture.applied.isEmpty());
}

void HybridGroupedGeometryReconcilerTest::
    followsUpdatedMaximizedTargetsAcrossWorkAreaChanges()
{
    Fixture fixture;
    fixture.windows = {
        {.windowId = QStringLiteral("editor"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = QRectF(0, 30, 960, 978),
         .targetFrame = QRectF(0, 30, 960, 978)},
        {.windowId = QStringLiteral("settings"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = QRectF(960, 30, 960, 978),
         .targetFrame = QRectF(960, 30, 960, 978)},
    };

    QVERIFY(fixture.reconciler.reconcile().isEmpty());
    QVERIFY(fixture.applied.isEmpty());

    fixture.windows[0].targetFrame = QRectF(0, 30, 960, 906);
    fixture.windows[1].targetFrame = QRectF(960, 30, 960, 906);
    QVERIFY(fixture.reconciler.reconcile().isEmpty());
    QCOMPARE(fixture.applied,
             QStringList({QStringLiteral("editor"), QStringLiteral("settings")}));
    QCOMPARE(fixture.windows[0].requestedFrame, fixture.windows[0].targetFrame);
    QCOMPARE(fixture.windows[1].requestedFrame, fixture.windows[1].targetFrame);
}

void HybridGroupedGeometryReconcilerTest::
    reportsInvalidAndRejectedMembersWithoutStoppingPeers()
{
    Fixture fixture;
    fixture.windows = {
        {.windowId = QStringLiteral("z-rejected"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = QRectF(1, 1, 10, 10),
         .targetFrame = QRectF(2, 2, 10, 10)},
        {.windowId = QStringLiteral("invalid"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = QRectF(1, 1, 10, 10),
         .targetFrame = {}},
        {.windowId = QStringLiteral("a-applied"),
         .containerId = QStringLiteral("group"),
         .requestedFrame = QRectF(1, 1, 10, 10),
         .targetFrame = QRectF(3, 3, 10, 10)},
    };
    fixture.rejected.insert(QStringLiteral("z-rejected"));

    const auto failures = fixture.reconciler.reconcile();
    QCOMPARE(fixture.applied,
             QStringList({QStringLiteral("a-applied"),
                          QStringLiteral("z-rejected")}));
    QCOMPARE(failures.size(), 2);
    QCOMPARE(failures[0],
             QStringLiteral("owned window 'invalid' has no valid target frame"));
    QCOMPARE(failures[1],
             QStringLiteral("window 'z-rejected': apply sentinel"));
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(
    QindaQt::Compositor::KWinIntegration::HybridGroupedGeometryReconcilerTest)

#include "tst_hybridgroupedgeometryreconciler.moc"
