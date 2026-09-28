// SPDX-License-Identifier: GPL-3.0-or-later
//
// Leaving whole-container maximize by acting on the frame (ADR-0282): a title
// drag restores under the pointer, a resize starts from the maximized frame,
// and a maximized container rolls up and unrolls maximized. Split from
// tst_hybridcontainerplacement.cpp by behaviour, sharing its fixture, the same
// way hybridcontainermaximize.cpp is split from the controller.

#include "hybridcontainerplacement_fixture.h"

#include <QtTest>

namespace QindaQt::Compositor::KWinIntegration {
using namespace PlacementFixtures;

namespace {

const QString Group = QStringLiteral("group");
// Fixture: restore frame (100,100) 800x600, work area (0,0) 1920x1040.
const QRect RestoreFrame(100, 100, 800, 600);

HybridInput::InteractionIntent pointerMove(HybridInput::IntentPhase phase,
                                           QPointF position, QPointF delta)
{
    auto intent = moveIntent(phase, delta);
    intent.position = position;
    return intent;
}

} // namespace

class HybridContainerMaximizeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void titleDragRestoresUnderThePointerAndKeepsMoving();
    void cancelledTitleDragReturnsToMaximized();
    void titleDragWithoutAPointerOnTheFrameCentersTheRestore();
    void refusedRestoreKeepsTheContainerMaximized();
    void resizeLeavesMaximizeFromTheMaximizedFrame();
    void maximizedContainerRollsUpAndUnrollsIntoTheCurrentArea();
    void movingARolledUpMaximizedStripLeavesMaximize();
    void restoringWhileRolledUpRetargetsTheUnroll();
};

// The owner's session log: a maximized container refused every move and
// roll-up, which read as a container that had stopped working. Every other
// desktop restores a maximized window when its title is dragged.
void HybridContainerMaximizeTest::titleDragRestoresUnderThePointerAndKeepsMoving()
{
    Fixture fixture;
    QVERIFY(fixture.controller.maximize(Group));
    QCOMPARE(fixture.layout.outerFrame, fixture.workArea);

    // Pressed at the middle of the maximized title; Begin arrives after the
    // drag threshold, 8 px further down.
    const auto begin = fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Begin, QPointF(960, 10), QPointF(0, 8)));
    QVERIFY2(begin.accepted, qPrintable(begin.message));
    QVERIFY(!fixture.controller.isMaximized(Group));
    // Restore size, the press point at the same fraction of the title width
    // (half of 800 = 400 left of x=960), the title row where it was plus the
    // drag so far.
    QCOMPARE(fixture.layout.outerFrame, QRect(560, 8, 800, 600));

    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Update, QPointF(1060, 60), QPointF(100, 58)))
                .accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(660, 58, 800, 600));
    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Commit, QPointF(1060, 60), QPointF(100, 58)))
                .accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(660, 58, 800, 600));
    QVERIFY(!fixture.controller.isMaximized(Group));

    // A plain move afterwards is an ordinary move from where it now is.
    QVERIFY(fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Commit, QPointF(10, 0))).accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(670, 58, 800, 600));
}

void HybridContainerMaximizeTest::cancelledTitleDragReturnsToMaximized()
{
    Fixture fixture;
    QVERIFY(fixture.controller.maximize(Group));
    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Begin, QPointF(200, 10), QPointF(8, 0)))
                .accepted);
    QVERIFY(!fixture.controller.isMaximized(Group));
    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Cancel, QPointF(300, 50), QPointF(100, 40)))
                .accepted);
    QVERIFY(fixture.controller.isMaximized(Group));
    QCOMPARE(fixture.layout.outerFrame, fixture.workArea);
    // The original restore frame survived the round trip.
    QVERIFY(fixture.controller.restore(Group));
    QCOMPARE(fixture.layout.outerFrame, RestoreFrame);
}

void HybridContainerMaximizeTest::titleDragWithoutAPointerOnTheFrameCentersTheRestore()
{
    Fixture fixture;
    fixture.workArea = QRect(0, 32, 1920, 1008);
    QVERIFY(fixture.controller.maximize(Group));
    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Begin, QPointF(), QPointF())).accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(560, 32, 800, 600));
    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Commit, QPointF(), QPointF())).accepted);
}

void HybridContainerMaximizeTest::refusedRestoreKeepsTheContainerMaximized()
{
    Fixture fixture;
    QVERIFY(fixture.controller.maximize(Group));
    fixture.failNext = true;
    const auto begin = fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Begin, QPointF(960, 10), QPointF(0, 8)));
    QVERIFY(!begin.accepted);
    QVERIFY(fixture.controller.isMaximized(Group));
    QCOMPARE(fixture.layout.outerFrame, fixture.workArea);
    // The rest of the refused gesture is absorbed; the next one works.
    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Commit, QPointF(960, 60), QPointF(0, 58)))
                .accepted);
    QVERIFY(fixture.controller.handleMove(
        pointerMove(HybridInput::IntentPhase::Begin, QPointF(960, 10), QPointF(0, 8)))
                .accepted);
    QVERIFY(!fixture.controller.isMaximized(Group));
}

void HybridContainerMaximizeTest::resizeLeavesMaximizeFromTheMaximizedFrame()
{
    Fixture fixture;
    QVERIFY(fixture.controller.maximize(Group));
    const auto begin = fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin));
    QVERIFY2(begin.accepted, qPrintable(begin.message));
    QVERIFY(!fixture.controller.isMaximized(Group));
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Update, QPointF(-100, -40))).accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(0, 0, 1820, 1000));

    // Cancel goes back to maximized with the original restore frame.
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Cancel, QPointF(-100, -40))).accepted);
    QVERIFY(fixture.controller.isMaximized(Group));
    QCOMPARE(fixture.layout.outerFrame, fixture.workArea);

    // Commit keeps the resized frame and leaves maximize for good.
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleResize(
        resizeIntent(HybridInput::IntentPhase::Commit, QPointF(-120, -40))).accepted);
    QCOMPARE(fixture.layout.outerFrame, QRect(0, 0, 1800, 1000));
    QVERIFY(!fixture.controller.isMaximized(Group));
}

// Outcome C of the 2026-09-28 round: a maximized container rolls up from its
// title (wheel, double-click, menu) and rolls back down maximized.
void HybridContainerMaximizeTest::maximizedContainerRollsUpAndUnrollsIntoTheCurrentArea()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.maximize(Group));
    const auto reflowsBefore = fixture.requestedFrames.size();
    QVERIFY2(fixture.controller.shade(Group, &error), qPrintable(error));
    QVERIFY(fixture.controller.isShaded(Group));
    QVERIFY(fixture.controller.isMaximized(Group));
    QCOMPARE(fixture.controller.shadedFrame(Group)->topLeft(), fixture.workArea.topLeft());
    // Rolling up never reflows members (ADR-0099)...
    QCOMPARE(fixture.requestedFrames.size(), reflowsBefore);

    // ...not even when the maximize area changes while rolled up.
    fixture.workArea = QRect(0, 32, 1920, 1008);
    QCOMPARE(fixture.controller.refreshMaximizedAreas(), QStringList{});
    QCOMPARE(fixture.requestedFrames.size(), reflowsBefore);

    // Unrolling returns to the maximize area as it is now, still maximized.
    QVERIFY(fixture.controller.unshade(Group, &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(0, 32, 1920, 1008));
    QVERIFY(fixture.controller.isMaximized(Group));
    QVERIFY(fixture.controller.restore(Group));
    QCOMPARE(fixture.layout.outerFrame, RestoreFrame);
}

void HybridContainerMaximizeTest::movingARolledUpMaximizedStripLeavesMaximize()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.maximize(Group));
    QVERIFY(fixture.controller.shade(Group, &error));

    // A cancelled strip drag changes nothing.
    QVERIFY(fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Cancel, QPointF(50, 40))).accepted);
    QVERIFY(fixture.controller.isMaximized(Group));

    // A committed one leaves maximize: the group reappears at its restore
    // size where the strip was put.
    QVERIFY(fixture.controller.handleMove(moveIntent(HybridInput::IntentPhase::Begin)).accepted);
    QVERIFY(fixture.controller.handleMove(
        moveIntent(HybridInput::IntentPhase::Commit, QPointF(50, 40))).accepted);
    QVERIFY(!fixture.controller.isMaximized(Group));
    QVERIFY(fixture.controller.unshade(Group, &error));
    QCOMPARE(fixture.layout.outerFrame, QRect(50, 40, 800, 600));
}

void HybridContainerMaximizeTest::restoringWhileRolledUpRetargetsTheUnroll()
{
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.maximize(Group));
    QVERIFY(fixture.controller.shade(Group, &error));
    const auto reflowsBefore = fixture.requestedFrames.size();

    QVERIFY2(fixture.controller.restore(Group, &error), qPrintable(error));
    QVERIFY(!fixture.controller.isMaximized(Group));
    QVERIFY(fixture.controller.isShaded(Group));
    QCOMPARE(fixture.requestedFrames.size(), reflowsBefore);
    QCOMPARE(fixture.controller.shadedFrame(Group)->topLeft(), RestoreFrame.topLeft());

    QVERIFY(fixture.controller.unshade(Group, &error));
    QCOMPARE(fixture.layout.outerFrame, RestoreFrame);
}

} // namespace QindaQt::Compositor::KWinIntegration

QTEST_GUILESS_MAIN(QindaQt::Compositor::KWinIntegration::HybridContainerMaximizeTest)
#include "tst_hybridcontainermaximize.moc"
