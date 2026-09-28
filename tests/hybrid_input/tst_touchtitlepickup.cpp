// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0282: a finger held still on a window's own title bar picks the window
// up for docking. Synthetic touch sequences through the pure policy, and the
// picked-up drag through InteractionController exactly as the KWin adapter
// drives it.

#include "interactiontestsupport.h"

#include "qindaqt/hybrid_input/touchtitlepickup.h"

#include <QtTest>

using namespace QindaQt::HybridInput;
using namespace QindaQt::HybridInput::TestSupport;

namespace {

const QPointF Title(200, 12);

PointerEvent fingerMove(QPointF position)
{
    return {.position = position, .changedButton = Qt::NoButton, .buttons = Qt::LeftButton,
            .modifiers = Qt::NoModifier};
}

PointerEvent fingerLift(QPointF position)
{
    return {.position = position, .changedButton = Qt::LeftButton, .buttons = Qt::NoButton,
            .modifiers = Qt::NoModifier};
}

} // namespace

class TouchTitlePickupTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void holdingStillPicksTheWindowUp();
    void aTapStaysKWins();
    void anOrdinaryTitleDragStaysKWins();
    void smallJitterStillCountsAsHolding();
    void aSecondFingerBeforeTheLongPressDisarms();
    void aSecondFingerAfterPickUpCancels();
    void aRefusedTakeOverLeavesTheSequenceToKWin();
    void aFingerOffTheTitleNeverArms();
    void aSeatCancelEndsThePickUp();
    void aStaleSequenceDoesNotEatTheNextFinger();
    void thePickedUpDragDocksLikeMetaShiftDrag();
    void aPickedUpIndependentWindowDroppedNowhereStaysPut();
};

void TouchTitlePickupTest::holdingStillPicksTheWindowUp()
{
    TouchTitlePickup pickup;
    const auto down = pickup.down(1, Title, 1000, true);
    QVERIFY(!down.consumed);
    QVERIFY(pickup.armed());
    QCOMPARE(pickup.longPressDueMs(), std::optional<qint64>(1500));
    // Not yet due.
    QCOMPARE(pickup.expire(1499, true).action, TouchPickupAction::None);
    const auto fired = pickup.expire(1500, true);
    QCOMPARE(fired.action, TouchPickupAction::PickUp);
    QCOMPARE(fired.position, Title);
    QVERIFY(pickup.pickedUp());

    const auto moved = pickup.motion(1, QPointF(400, 300), 1600);
    QCOMPARE(moved.action, TouchPickupAction::Move);
    QVERIFY(moved.consumed);
    // Other fingers' motion is not ours.
    QCOMPARE(pickup.motion(7, QPointF(1, 1), 1610).action, TouchPickupAction::None);

    const auto dropped = pickup.up(1, 1700);
    QCOMPARE(dropped.action, TouchPickupAction::Drop);
    QCOMPARE(dropped.position, QPointF(400, 300));
    // The lift must still reach KWin's decoration filter.
    QVERIFY(!dropped.consumed);
    QVERIFY(!pickup.tracking());
}

void TouchTitlePickupTest::aTapStaysKWins()
{
    TouchTitlePickup pickup;
    QVERIFY(!pickup.down(1, Title, 0, true).consumed);
    const auto lifted = pickup.up(1, 120);
    QCOMPARE(lifted.action, TouchPickupAction::None);
    QVERIFY(!lifted.consumed);
    QVERIFY(!pickup.tracking());
    QCOMPARE(pickup.expire(500, true).action, TouchPickupAction::None);
}

void TouchTitlePickupTest::anOrdinaryTitleDragStaysKWins()
{
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    const auto moved = pickup.motion(1, Title + QPointF(30, 0), 100);
    QVERIFY(!moved.consumed);
    QVERIFY(!pickup.armed());
    QVERIFY(!pickup.longPressDueMs().has_value());
    // Holding still afterwards changes nothing: KWin's move owns it.
    QCOMPARE(pickup.expire(600, true).action, TouchPickupAction::None);
    QVERIFY(!pickup.motion(1, Title + QPointF(90, 40), 700).consumed);
    QCOMPARE(pickup.up(1, 800).action, TouchPickupAction::None);
}

void TouchTitlePickupTest::smallJitterStillCountsAsHolding()
{
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    QVERIFY(!pickup.motion(1, Title + QPointF(3, -2), 200).consumed);
    QVERIFY(pickup.armed());
    const auto fired = pickup.expire(500, true);
    QCOMPARE(fired.action, TouchPickupAction::PickUp);
    QCOMPARE(fired.position, Title + QPointF(3, -2));
}

void TouchTitlePickupTest::aSecondFingerBeforeTheLongPressDisarms()
{
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    const auto second = pickup.down(2, QPointF(600, 400), 100, false);
    QVERIFY(!second.consumed);
    QVERIFY(!pickup.armed());
    QCOMPARE(pickup.expire(500, true).action, TouchPickupAction::None);
    static_cast<void>(pickup.up(2, 200));
    QVERIFY(pickup.tracking());
    static_cast<void>(pickup.up(1, 300));
    QVERIFY(!pickup.tracking());
}

void TouchTitlePickupTest::aSecondFingerAfterPickUpCancels()
{
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    static_cast<void>(pickup.expire(500, true));
    const auto second = pickup.down(2, QPointF(600, 400), 600, false);
    QCOMPARE(second.action, TouchPickupAction::Cancel);
    QVERIFY(!second.consumed);
    // The first finger is passed through from now on and its lift drops nothing.
    QVERIFY(!pickup.motion(1, QPointF(300, 300), 700).consumed);
    QCOMPARE(pickup.up(1, 800).action, TouchPickupAction::None);
    QCOMPARE(pickup.up(2, 900).action, TouchPickupAction::None);
    QVERIFY(!pickup.tracking());
}

void TouchTitlePickupTest::aRefusedTakeOverLeavesTheSequenceToKWin()
{
    // The adapter refuses when KWin already started its own move, or the
    // finger is on a title-bar button.
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    QCOMPARE(pickup.expire(500, false).action, TouchPickupAction::None);
    QVERIFY(!pickup.pickedUp());
    QVERIFY(!pickup.motion(1, QPointF(300, 300), 600).consumed);
    QCOMPARE(pickup.up(1, 700).action, TouchPickupAction::None);
}

void TouchTitlePickupTest::aFingerOffTheTitleNeverArms()
{
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, QPointF(300, 300), 0, false));
    QVERIFY(!pickup.armed());
    QCOMPARE(pickup.expire(500, true).action, TouchPickupAction::None);
    // A later finger on a title while the first is still down does not arm
    // either: only the first finger of a sequence can pick up.
    static_cast<void>(pickup.down(2, Title, 100, true));
    QVERIFY(!pickup.armed());
}

void TouchTitlePickupTest::aSeatCancelEndsThePickUp()
{
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    static_cast<void>(pickup.expire(500, true));
    QCOMPARE(pickup.cancel().action, TouchPickupAction::Cancel);
    QVERIFY(!pickup.tracking());
    QCOMPARE(pickup.cancel().action, TouchPickupAction::None);
}

void TouchTitlePickupTest::aStaleSequenceDoesNotEatTheNextFinger()
{
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, QPointF(300, 300), 0, false));
    // The lift was never delivered; a fresh finger arrives much later.
    static_cast<void>(pickup.down(2, Title, 10000, true));
    QVERIFY(pickup.armed());
}

// The adapter adopts the same dock drag as Meta + Shift + left drag, so the
// same drop targets highlight and the same drop rules apply.
void TouchTitlePickupTest::thePickedUpDragDocksLikeMetaShiftDrag()
{
    RecordingResolver resolver;
    resolver.pointerTarget = {QStringLiteral("group"), QStringLiteral("peer"), DockZone::Tab};
    InteractionController controller(resolver);
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    const auto fired = pickup.expire(500, true);
    const HitTarget source{HitKind::MemberTitle, {}, QStringLiteral("editor"), {}};
    const auto adopted = controller.adoptDrag(source, fired.position);
    QCOMPARE(adopted.intents.size(), qsizetype{2});
    QCOMPARE(adopted.intents[0].kind, InteractionKind::MemberDock);
    QCOMPARE(adopted.intents[0].phase, IntentPhase::Begin);
    QCOMPARE(adopted.intents[1].target, resolver.pointerTarget);

    const auto moved = pickup.motion(1, QPointF(500, 400), 600);
    QVERIFY(controller.pointerMove(fingerMove(moved.position)).consumed);
    const auto dropped = pickup.up(1, 700);
    const auto committed = controller.pointerRelease(fingerLift(dropped.position));
    QCOMPARE(committed.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(committed.intents.constFirst().target, resolver.pointerTarget);
    QCOMPARE(committed.intents.constFirst().source.memberId, QStringLiteral("editor"));
    QVERIFY(!controller.active());
}

void TouchTitlePickupTest::aPickedUpIndependentWindowDroppedNowhereStaysPut()
{
    RecordingResolver resolver; // no drop target anywhere
    InteractionController controller(resolver);
    TouchTitlePickup pickup;
    static_cast<void>(pickup.down(1, Title, 0, true));
    const auto fired = pickup.expire(500, true);
    static_cast<void>(controller.adoptDrag(
        HitTarget{HitKind::MemberTitle, {}, QStringLiteral("editor"), {}}, fired.position));
    const auto dropped = pickup.up(1, 600);
    const auto released = controller.pointerRelease(fingerLift(dropped.position));
    QCOMPARE(released.intents.constFirst().phase, IntentPhase::Cancel);

    // A second finger mid-drag cancels the controller's gesture too.
    TouchTitlePickup again;
    static_cast<void>(again.down(1, Title, 1000, true));
    static_cast<void>(controller.adoptDrag(
        HitTarget{HitKind::MemberTitle, QStringLiteral("group"), QStringLiteral("m"), {}},
        again.expire(1500, true).position));
    QCOMPARE(again.down(2, QPointF(9, 9), 1600, false).action, TouchPickupAction::Cancel);
    QCOMPARE(controller.cancel().intents.constFirst().phase, IntentPhase::Cancel);
}

QTEST_GUILESS_MAIN(TouchTitlePickupTest)
#include "tst_touchtitlepickup.moc"
