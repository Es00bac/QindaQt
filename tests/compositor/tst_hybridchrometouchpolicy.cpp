// SPDX-License-Identifier: GPL-3.0-or-later
// The pure touch policy for shared chrome (ADR-0193): what a finger means.
#include "hybridchrometouchpolicy.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class HybridChromeTouchPolicyTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ignoresFingersOffChrome();
    void tapIsPressThenRelease();
    void heldFingerBecomesLongPressAndTheLiftIsSpent();
    void movementBeyondSlopDisarmsTheLongPressAndDrags();
    void secondFingerSwipesRollUpAndDownOnRollTargets();
    void secondFingerOnNonRollTargetSpendsTheGesture();
    void primaryLiftingFirstSpendsTheSequenceAndKeepsConsuming();
    void fingersOffTheChromeAreNeverConsumedWhileAGestureRuns();
    void staleSequenceIsAbandonedByTheNextFinger();
    void cancelClearsEverything();
};

void HybridChromeTouchPolicyTests::ignoresFingersOffChrome()
{
    HybridChromeTouchPolicy policy;
    const auto down = policy.down(1, {10.0, 10.0}, 0, false, false);
    QCOMPARE(down.gesture, TouchGesture::None);
    QVERIFY(!down.consumed);
    QVERIFY(!policy.active());
    QVERIFY(!policy.motion(1, {12.0, 10.0}, 10).consumed);
    QVERIFY(!policy.up(1, 20).consumed);
    QVERIFY(!policy.longPressDueMs().has_value());
}

void HybridChromeTouchPolicyTests::tapIsPressThenRelease()
{
    HybridChromeTouchPolicy policy;
    const auto down = policy.down(1, {10.0, 10.0}, 1000, true, true);
    QCOMPARE(down.gesture, TouchGesture::Press);
    QCOMPARE(down.position, QPointF(10.0, 10.0));
    QVERIFY(down.consumed);
    QVERIFY(policy.active());
    QCOMPARE(policy.longPressDueMs(), std::optional<qint64>(1500));
    // A few pixels of wobble stay within the slop and keep the long press armed.
    const auto wobble = policy.motion(1, {13.0, 12.0}, 1050);
    QCOMPARE(wobble.gesture, TouchGesture::Move);
    QVERIFY(policy.longPressDueMs().has_value());
    QVERIFY(!policy.expire(1400).has_value());
    const auto up = policy.up(1, 1100);
    QCOMPARE(up.gesture, TouchGesture::Release);
    QCOMPARE(up.position, QPointF(13.0, 12.0));
    QVERIFY(up.consumed);
    QVERIFY(!policy.active());
}

void HybridChromeTouchPolicyTests::heldFingerBecomesLongPressAndTheLiftIsSpent()
{
    HybridChromeTouchPolicy policy({.longPressMs = 300});
    QCOMPARE(policy.down(1, {40.0, 20.0}, 0, true, true).gesture, TouchGesture::Press);
    QVERIFY(!policy.expire(299).has_value());
    const auto held = policy.expire(300);
    QVERIFY(held.has_value());
    QCOMPARE(held->gesture, TouchGesture::LongPress);
    QCOMPARE(held->position, QPointF(40.0, 20.0));
    QVERIFY(!policy.expire(400).has_value());
    QVERIFY(!policy.longPressDueMs().has_value());
    // After the menu opened, moving and lifting must not drag or click.
    const auto move = policy.motion(1, {90.0, 20.0}, 350);
    QCOMPARE(move.gesture, TouchGesture::None);
    QVERIFY(move.consumed);
    const auto up = policy.up(1, 400);
    QCOMPARE(up.gesture, TouchGesture::None);
    QVERIFY(up.consumed);
    QVERIFY(!policy.active());
}

void HybridChromeTouchPolicyTests::movementBeyondSlopDisarmsTheLongPressAndDrags()
{
    HybridChromeTouchPolicy policy;
    QCOMPARE(policy.down(1, {10.0, 10.0}, 0, true, true).gesture, TouchGesture::Press);
    const auto move = policy.motion(1, {30.0, 10.0}, 100);
    QCOMPARE(move.gesture, TouchGesture::Move);
    QCOMPARE(move.position, QPointF(30.0, 10.0));
    QVERIFY(!policy.longPressDueMs().has_value());
    QVERIFY(!policy.expire(1000).has_value());
    QCOMPARE(policy.motion(1, {60.0, 12.0}, 200).gesture, TouchGesture::Move);
    const auto up = policy.up(1, 300);
    QCOMPARE(up.gesture, TouchGesture::Release);
    QCOMPARE(up.position, QPointF(60.0, 12.0));
}

void HybridChromeTouchPolicyTests::secondFingerSwipesRollUpAndDownOnRollTargets()
{
    HybridChromeTouchPolicy policy;
    QCOMPARE(policy.down(1, {100.0, 100.0}, 0, true, true).gesture, TouchGesture::Press);
    const auto second = policy.down(2, {140.0, 100.0}, 50, true, true);
    QCOMPARE(second.gesture, TouchGesture::None);
    QVERIFY(second.consumed);
    QVERIFY(!policy.longPressDueMs().has_value());
    // Below the swipe distance nothing happens and nothing drags.
    QCOMPARE(policy.motion(2, {140.0, 80.0}, 80).gesture, TouchGesture::None);
    QCOMPARE(policy.motion(1, {100.0, 90.0}, 90).gesture, TouchGesture::None);
    const auto swipe = policy.motion(2, {140.0, 55.0}, 120);
    QCOMPARE(swipe.gesture, TouchGesture::SwipeUp);
    QVERIFY(swipe.consumed);
    // The rest of the sequence is spent.
    QCOMPARE(policy.motion(2, {140.0, 20.0}, 150).gesture, TouchGesture::None);
    QCOMPARE(policy.up(2, 200).gesture, TouchGesture::None);
    QCOMPARE(policy.up(1, 210).gesture, TouchGesture::None);
    QVERIFY(!policy.active());

    HybridChromeTouchPolicy down;
    QCOMPARE(down.down(7, {100.0, 100.0}, 0, true, true).gesture, TouchGesture::Press);
    QVERIFY(down.down(8, {130.0, 100.0}, 10, true, true).consumed);
    QCOMPARE(down.motion(7, {100.0, 150.0}, 60).gesture, TouchGesture::SwipeDown);
}

void HybridChromeTouchPolicyTests::secondFingerOnNonRollTargetSpendsTheGesture()
{
    HybridChromeTouchPolicy policy;
    QCOMPARE(policy.down(1, {10.0, 10.0}, 0, true, false).gesture, TouchGesture::Press);
    QVERIFY(policy.down(2, {40.0, 10.0}, 20, true, false).consumed);
    // No swipe on a divider; and once two fingers touched, no drag either.
    QCOMPARE(policy.motion(2, {40.0, 200.0}, 40).gesture, TouchGesture::None);
    QCOMPARE(policy.up(2, 50).gesture, TouchGesture::None);
    QCOMPARE(policy.motion(1, {80.0, 10.0}, 60).gesture, TouchGesture::None);
    const auto up = policy.up(1, 70);
    QCOMPARE(up.gesture, TouchGesture::None);
    QVERIFY(up.consumed);
    QVERIFY(!policy.active());
}

void HybridChromeTouchPolicyTests::primaryLiftingFirstSpendsTheSequenceAndKeepsConsuming()
{
    // Two fingers on a tab, a swipe that falls short, the FRONT finger lifts
    // first: no click, and the finger still down stays ours until it lifts.
    HybridChromeTouchPolicy policy;
    QCOMPARE(policy.down(1, {100.0, 100.0}, 0, true, true).gesture, TouchGesture::Press);
    QVERIFY(policy.down(2, {140.0, 100.0}, 50, true, true).consumed);
    QCOMPARE(policy.motion(2, {140.0, 80.0}, 80).gesture, TouchGesture::None);
    const auto primaryUp = policy.up(1, 120);
    QCOMPARE(primaryUp.gesture, TouchGesture::None);
    QVERIFY(primaryUp.consumed);
    QVERIFY(policy.active());
    QVERIFY(!policy.longPressDueMs().has_value());
    QVERIFY(!policy.expire(2000).has_value());
    const auto stillDown = policy.motion(2, {150.0, 70.0}, 140);
    QCOMPARE(stillDown.gesture, TouchGesture::None);
    QVERIFY(stillDown.consumed);
    // The lifted primary's id is a foreign finger now.
    QVERIFY(!policy.motion(1, {10.0, 10.0}, 150).consumed);
    QVERIFY(!policy.down(1, {10.0, 10.0}, 160, true, true).consumed || policy.active());
    const auto secondaryUp = policy.up(2, 200);
    QCOMPARE(secondaryUp.gesture, TouchGesture::None);
    QVERIFY(secondaryUp.consumed);
    QVERIFY(!policy.active());
    // Mutation guard: deleting the spent rule in up() would have produced a
    // Release above; a fresh tap afterwards still clicks.
    QCOMPARE(policy.down(3, {100.0, 100.0}, 300, true, true).gesture, TouchGesture::Press);
    QCOMPARE(policy.up(3, 320).gesture, TouchGesture::Release);
}

void HybridChromeTouchPolicyTests::fingersOffTheChromeAreNeverConsumedWhileAGestureRuns()
{
    HybridChromeTouchPolicy policy;
    QCOMPARE(policy.down(1, {100.0, 100.0}, 0, true, true).gesture, TouchGesture::Press);
    // A finger on a client window (not over chrome) is KWin's: not consumed,
    // and it neither joins nor spends the gesture.
    const auto foreignDown = policy.down(9, {600.0, 400.0}, 20, false, false);
    QCOMPARE(foreignDown.gesture, TouchGesture::None);
    QVERIFY(!foreignDown.consumed);
    QVERIFY(!policy.motion(9, {620.0, 410.0}, 30).consumed);
    QVERIFY(!policy.up(9, 40).consumed);
    QVERIFY(policy.longPressDueMs().has_value());
    // The chrome finger still taps normally afterwards.
    const auto up = policy.up(1, 100);
    QCOMPARE(up.gesture, TouchGesture::Release);
    QVERIFY(up.consumed);
    QVERIFY(!policy.active());
}

void HybridChromeTouchPolicyTests::staleSequenceIsAbandonedByTheNextFinger()
{
    HybridChromeTouchPolicy policy({.staleSequenceMs = 1000});
    QCOMPARE(policy.down(1, {100.0, 100.0}, 0, true, true).gesture, TouchGesture::Press);
    // The grab was stolen: no up, no cancel. Within the window the gesture
    // still owns its finger; past it the owner sees it as stale.
    QVERIFY(!policy.isStale(900));
    QVERIFY(policy.isStale(1001));
    // A much later finger over chrome starts fresh instead of being eaten.
    const auto later = policy.down(5, {100.0, 100.0}, 5000, true, true);
    QCOMPARE(later.gesture, TouchGesture::Press);
    QVERIFY(later.consumed);
    QVERIFY(policy.active());
    QCOMPARE(policy.longPressDueMs(), std::optional<qint64>(5500));
    QCOMPARE(policy.up(5, 5100).gesture, TouchGesture::Release);
    // A later finger off chrome after a stale gesture is not consumed either.
    QCOMPARE(policy.down(6, {100.0, 100.0}, 6000, true, true).gesture, TouchGesture::Press);
    QVERIFY(!policy.down(7, {700.0, 500.0}, 9000, false, false).consumed);
    QVERIFY(!policy.active());
}

void HybridChromeTouchPolicyTests::cancelClearsEverything()
{
    HybridChromeTouchPolicy policy;
    QVERIFY(!policy.cancel().consumed);
    QCOMPARE(policy.down(1, {10.0, 10.0}, 0, true, true).gesture, TouchGesture::Press);
    const auto cancelled = policy.cancel();
    QCOMPARE(cancelled.gesture, TouchGesture::Cancel);
    QVERIFY(cancelled.consumed);
    QVERIFY(!policy.active());
    QVERIFY(!policy.up(1, 10).consumed);
    // A fresh gesture starts clean after a cancel.
    QCOMPARE(policy.down(1, {10.0, 10.0}, 20, true, true).gesture, TouchGesture::Press);
    QCOMPARE(policy.longPressDueMs(), std::optional<qint64>(520));
}

QTEST_GUILESS_MAIN(HybridChromeTouchPolicyTests)
#include "tst_hybridchrometouchpolicy.moc"
