// SPDX-License-Identifier: GPL-3.0-or-later
//
// What happens to a container when the world changes underneath it: an output
// it lived on is removed, or a gesture it started is refused. Split from
// tst_hybridcontainerplacement.cpp by behaviour, sharing its fixture.

#include "hybridcontainerplacement_fixture.h"

#include <QtTest>

namespace QindaQt::Compositor::KWinIntegration {
using namespace PlacementFixtures;

class HybridContainerRescueTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reflowsContainersStrandedOffEveryOutput();
    void absorbsTheRestOfAGestureWhoseBeginWasRefused();
};

void HybridContainerRescueTest::reflowsContainersStrandedOffEveryOutput()
{
    Fixture fixture;

    // A container that is still reachable is left completely alone.
    fixture.layout.outerFrame = QRect(100, 100, 800, 600);
    fixture.requestedFrames.clear();
    QVERIFY(fixture.controller.refreshStrandedContainers().isEmpty());
    QVERIFY(fixture.requestedFrames.isEmpty());
    QCOMPARE(fixture.layout.outerFrame, QRect(100, 100, 800, 600));

    // The live shape, with the geometry measured off the running session:
    // work area (0,32) 1920x1048 after DP-1 was removed, and
    // hybrid-r133-container left at (0,1104) 1918x1046.
    fixture.workArea = QRect(0, 32, 1920, 1048);
    fixture.layout.outerFrame = QRect(0, 1104, 1918, 1046);
    fixture.requestedFrames.clear();
    QVERIFY(fixture.controller.refreshStrandedContainers().isEmpty());
    QCOMPARE(fixture.requestedFrames.size(), 1);

    // Moved, not resized: it still fits, so the user keeps their layout.
    const QRect moved = fixture.layout.outerFrame;
    QCOMPARE(moved.size(), QSize(1918, 1046));
    QVERIFY2(moved.intersects(fixture.workArea), qPrintable(QStringLiteral(
        "stranded container was not brought back: %1,%2 %3x%4")
            .arg(moved.x()).arg(moved.y()).arg(moved.width()).arg(moved.height())));
    QVERIFY(fixture.workArea.contains(moved));

    // Idempotent: a second pass has nothing to do.
    fixture.requestedFrames.clear();
    QVERIFY(fixture.controller.refreshStrandedContainers().isEmpty());
    QVERIFY(fixture.requestedFrames.isEmpty());

    // A container larger than the surviving work area is clamped to it rather
    // than left partly unreachable.
    fixture.layout.outerFrame = QRect(4000, 4000, 3000, 2000);
    fixture.requestedFrames.clear();
    QVERIFY(fixture.controller.refreshStrandedContainers().isEmpty());
    QCOMPARE(fixture.layout.outerFrame, fixture.workArea);

    // A maximized container stays the business of refreshMaximizedAreas().
    fixture.layout.outerFrame = QRect(100, 100, 800, 600);
    QVERIFY(fixture.controller.maximize(QStringLiteral("group")));
    fixture.layout.outerFrame = QRect(0, 1104, 1918, 1046);
    fixture.requestedFrames.clear();
    QVERIFY(fixture.controller.refreshStrandedContainers().isEmpty());
    QVERIFY2(fixture.requestedFrames.isEmpty(),
             "a maximized container must be left to refreshMaximizedAreas()");

    // A reflow failure is reported, not swallowed.
    QVERIFY(fixture.controller.restore(QStringLiteral("group")));
    fixture.layout.outerFrame = QRect(0, 1104, 1918, 1046);
    fixture.failNext = true;
    QCOMPARE(fixture.controller.refreshStrandedContainers().size(), 1);
}

// A container left entirely outside the work area when its display was
// removed. Nothing relocated it: refreshMaximizedAreas() only re-fits
// containers that are maximized, and the grouped geometry reconciler then
// reasserts the stale plan, so a KWin-level move of a member is undone. The
// user is left with a container whose title bar is off-screen and therefore
// cannot be dragged back, and every visibility snapshot is rejected because a
// managed window lies outside its own output. See OPEN-DEFECTS.md item 0.
// Dragging a maximized container refuses Begin, but the interaction controller
// keeps sending Update for the rest of the gesture. Re-reporting the refusal
// once per pointer motion event put 128 lines in one session log from a
// handful of drags, drowning the channel a real interaction failure uses.
// See OPEN-DEFECTS.md item 10.
void HybridContainerRescueTest::absorbsTheRestOfAGestureWhoseBeginWasRefused()
{
    Fixture fixture;
    const QString group = QStringLiteral("group");
    QVERIFY(fixture.controller.maximize(group));

    // Begin still says why, exactly once.
    auto begin = fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Begin));
    QVERIFY(!begin.accepted);
    QVERIFY(begin.message.contains(QStringLiteral("restore a maximized container")));
    QVERIFY(fixture.controller.gestureRefused(group));

    // Every subsequent motion is absorbed silently rather than re-reported.
    for (int motion = 0; motion != 5; ++motion) {
        const auto update = fixture.controller.handleMove(
            moveIntent(HybridInput::IntentPhase::Update, QPointF(motion, motion)));
        QVERIFY2(update.accepted, qPrintable(update.message));
        QVERIFY(update.message.isEmpty());
    }

    // The end of the gesture clears the suppression.
    const auto commit = fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Commit));
    QVERIFY(commit.accepted);
    QVERIFY(!fixture.controller.gestureRefused(group));

    // A genuinely missing baseline, with no refused Begin before it, is still
    // reported -- that is a real anomaly and must not be swallowed.
    const auto orphan = fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Update, QPointF(4, 4)));
    QVERIFY(!orphan.accepted);
    QCOMPARE(orphan.message, QStringLiteral("container move has no active baseline"));

    // A Begin that succeeds leaves no suppression behind.
    QVERIFY(fixture.controller.restore(group));
    QVERIFY(fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(!fixture.controller.gestureRefused(group));
    QVERIFY(fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Cancel)).accepted);
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::HybridContainerRescueTest)
#include "tst_hybridcontainerrescue.moc"
