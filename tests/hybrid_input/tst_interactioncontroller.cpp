// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_input/interactioncontroller.h"
#include "interactiontestsupport.h"

#include <QTest>

using namespace QindaQt::HybridInput;
using namespace QindaQt::HybridInput::TestSupport;

class InteractionControllerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void unrelatedInputPassesThrough();
    void pointerThresholdUsesTotalBaselineDisplacement();
    void memberDragPreviewsAndCommitsDock();
    void groupedMemberDragOutsideCommitsDetach();
    void independentDragOutsideCancels();
    void pointerGeometryKindsUseCumulativeDeltas();
    void externalCancelKeepsCumulativeDelta();
};

void InteractionControllerTest::unrelatedInputPassesThrough()
{
    RecordingResolver resolver;
    resolver.hit = {HitKind::MemberTitle, {}, QStringLiteral("window-a"), {}};
    InteractionController controller(resolver);

    QVERIFY(!controller.keyEvent({.key = Qt::Key_Left}).consumed);
    QVERIFY(!controller.keyEvent({.key = Qt::Key_Escape}).consumed);
    QVERIFY(!controller.pointerPress(pressAt({10, 10}, Qt::NoModifier)).consumed);
    QVERIFY(!controller.active());
    QVERIFY(!controller.pointerPress(
        pressAt({10, 10}, Qt::MetaModifier | Qt::ShiftModifier
                                  | Qt::AltModifier)).consumed);
    QVERIFY(!controller.active());
    resolver.hit = {};
    QVERIFY(!controller.pointerPress(
        pressAt({10, 10}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);

    resolver.hit = {HitKind::OuterTitle, QStringLiteral("group"), {}, {}};
    QVERIFY(controller.pointerPress(
        pressAt({10, 10}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);
    QVERIFY(!controller.keyEvent({.key = Qt::Key_Q}).consumed);
    QVERIFY(!controller.keyEvent({.key = Qt::Key_Escape, .pressed = false}).consumed);
    QVERIFY(controller.active());
    const auto escape = controller.keyEvent({.key = Qt::Key_Escape});
    QVERIFY(escape.consumed);
    QCOMPARE(escape.intents.constFirst().phase, IntentPhase::Cancel);
    QVERIFY(!controller.active());
}

void InteractionControllerTest::pointerThresholdUsesTotalBaselineDisplacement()
{
    RecordingResolver resolver;
    resolver.hit = {HitKind::OuterTitle, QStringLiteral("group"), {}, {}};
    InteractionController controller(resolver, {.dragThreshold = 5.0});

    QVERIFY(controller.pointerPress(
        pressAt({5, 5}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);
    const auto pending = controller.pointerMove({.position = {8, 5}});
    QVERIFY(pending.consumed);
    QVERIFY(pending.intents.isEmpty());

    const auto activated = controller.pointerMove({.position = {11, 8}});
    QCOMPARE(activated.intents.size(), 2);
    QCOMPARE(activated.intents[0].phase, IntentPhase::Begin);
    QCOMPARE(activated.intents[0].delta, QPointF(6, 3));
    QCOMPARE(activated.intents[1].phase, IntentPhase::Update);
    QCOMPARE(activated.intents[1].delta, QPointF(6, 3));

    const auto moved = controller.pointerMove({.position = {13, 9}});
    QCOMPARE(moved.intents.size(), 1);
    QCOMPARE(moved.intents.constFirst().delta, QPointF(8, 4));

    const auto released = controller.pointerRelease(releaseAt({15, 10}));
    QCOMPARE(released.intents.size(), 1);
    QCOMPARE(released.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(released.intents.constFirst().delta, QPointF(10, 5));
    QCOMPARE(released.intents.constFirst().position, QPointF(15, 10));
    QVERIFY(!controller.active());
}

void InteractionControllerTest::memberDragPreviewsAndCommitsDock()
{
    RecordingResolver resolver;
    resolver.hit = {HitKind::MemberTitle, {}, QStringLiteral("window-a"), {}};
    resolver.pointerTarget = {QStringLiteral("container-b"),
                              QStringLiteral("window-b"), DockZone::Right};
    InteractionController controller(resolver, {.dragThreshold = 5.0});

    QVERIFY(controller.pointerPress(
        pressAt({10, 10}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);
    const auto pending = controller.pointerMove({.position = {12, 10}});
    QVERIFY(pending.consumed);
    QVERIFY(pending.intents.isEmpty());

    const auto moved = controller.pointerMove({.position = {20, 10}});
    QCOMPARE(moved.intents.size(), 2);
    QCOMPARE(moved.intents[0].phase, IntentPhase::Begin);
    QCOMPARE(moved.intents[1].phase, IntentPhase::Update);
    QCOMPARE(moved.intents[1].target.zone, DockZone::Right);
    QCOMPARE(moved.intents[1].delta, QPointF(10, 0));

    const auto sameTarget = controller.pointerMove({.position = {25, 12}});
    QVERIFY(sameTarget.consumed);
    QVERIFY(sameTarget.intents.isEmpty());

    const auto released = controller.pointerRelease(releaseAt({30, 15}));
    QCOMPARE(released.intents.size(), 1);
    QCOMPARE(released.intents[0].phase, IntentPhase::Commit);
    QCOMPARE(released.intents[0].target.memberId, QStringLiteral("window-b"));
    QCOMPARE(released.intents[0].delta, QPointF(20, 5));
    QVERIFY(!controller.active());
}

void InteractionControllerTest::groupedMemberDragOutsideCommitsDetach()
{
    RecordingResolver resolver;
    resolver.hit = {HitKind::Tab, QStringLiteral("group"), QStringLiteral("window-a"), {}};
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    QVERIFY(controller.pointerPress(
        pressAt({0, 0}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);
    QVERIFY(controller.pointerMove({.position = {20, 0}}).consumed);
    const auto released = controller.pointerRelease(releaseAt({30, 0}));
    QCOMPARE(released.intents.constFirst().phase, IntentPhase::Commit);
    QVERIFY(!released.intents.constFirst().target.isValid());
    QCOMPARE(released.intents.constFirst().delta, QPointF(30, 0));
}

void InteractionControllerTest::independentDragOutsideCancels()
{
    RecordingResolver resolver;
    resolver.hit = {HitKind::MemberTitle, {}, QStringLiteral("window-a"), {}};
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    QVERIFY(controller.pointerPress(
        pressAt({0, 0}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);
    QVERIFY(controller.pointerMove({.position = {20, 0}}).consumed);
    const auto released = controller.pointerRelease(releaseAt({30, 0}));
    QCOMPARE(released.intents.constFirst().phase, IntentPhase::Cancel);
    QCOMPARE(released.intents.constFirst().delta, QPointF(30, 0));
}

void InteractionControllerTest::pointerGeometryKindsUseCumulativeDeltas()
{
    const QVector<HitTarget> sources{
        {HitKind::OuterTitle, QStringLiteral("group"), {}, {}},
        {HitKind::Divider, QStringLiteral("group"), {}, QStringLiteral("0/1")},
        {HitKind::OuterResize, QStringLiteral("group"), {}, {},
         Qt::RightEdge | Qt::BottomEdge},
    };
    const QVector<InteractionKind> kinds{
        InteractionKind::ContainerMove,
        InteractionKind::DividerResize,
        InteractionKind::ContainerResize,
    };
    for (qsizetype index = 0; index < sources.size(); ++index) {
        RecordingResolver resolver;
        resolver.hit = sources[index];
        InteractionController controller(resolver, {.dragThreshold = 1.0});
        QVERIFY(controller.pointerPress(
            pressAt({5, 5}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);
        const auto moved = controller.pointerMove({.position = {15, 17}});
        QCOMPARE(moved.intents.size(), 2);
        QCOMPARE(moved.intents[0].kind, kinds[index]);
        QCOMPARE(moved.intents[1].delta, QPointF(10, 12));
        const auto movedAgain = controller.pointerMove({.position = {20, 23}});
        QCOMPARE(movedAgain.intents.constFirst().delta, QPointF(15, 18));
        const auto released = controller.pointerRelease(releaseAt({21, 24}));
        QCOMPARE(released.intents.constFirst().phase, IntentPhase::Commit);
        QCOMPARE(released.intents.constFirst().delta, QPointF(16, 19));
    }
}

void InteractionControllerTest::externalCancelKeepsCumulativeDelta()
{
    RecordingResolver resolver;
    resolver.hit = {HitKind::OuterTitle, QStringLiteral("group"), {}, {}};
    InteractionController controller(resolver, {.dragThreshold = 0.0});
    QVERIFY(controller.pointerPress(
        pressAt({10, 20}, Qt::MetaModifier | Qt::ShiftModifier)).consumed);
    QVERIFY(controller.pointerMove({.position = {25, 35}}).consumed);
    const auto cancelled = controller.cancel();
    QVERIFY(cancelled.consumed);
    QCOMPARE(cancelled.intents.constFirst().phase, IntentPhase::Cancel);
    QCOMPARE(cancelled.intents.constFirst().delta, QPointF(15, 15));
    QVERIFY(!controller.active());
    QVERIFY(!controller.cancel().consumed);
}

QTEST_GUILESS_MAIN(InteractionControllerTest)
#include "tst_interactioncontroller.moc"
