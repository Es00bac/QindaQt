// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridiconchiprouter.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

IconChipPointerHit chipHit(HybridChrome::IconChipHitKind kind,
                           const QString &windowId = QStringLiteral("window-a"))
{
    return {windowId, kind};
}

HybridInput::PointerEvent pointer(QPointF position,
                                  Qt::MouseButton changedButton = Qt::NoButton,
                                  Qt::MouseButtons buttons = {},
                                  Qt::KeyboardModifiers modifiers = {})
{
    return {position, changedButton, buttons, modifiers};
}

} // namespace

class HybridIconChipRouterTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void passesModifiedPressesAndPointsOutsideChips();
    void raisesOnBodyPressAndUnrollsOnDoubleClickOnly();
    void closesOnAMatchingCloseRelease();
    void emitsThresholdedCumulativeDragLifecycle();
    void routesContextMenuOnMatchingRightRelease();
    void cancelsOwnedGrabAndClearsHoverOutsideChips();
    void wheelTowardTheUserUnrollsAndAwayIsSwallowed();
};

void HybridIconChipRouterTests::passesModifiedPressesAndPointsOutsideChips()
{
    std::optional<IconChipPointerHit> resolved;
    HybridIconChipRouter router([&](const QPointF &) { return resolved; });

    resolved = std::nullopt;
    QVERIFY(!router.pointerPress(pointer({1.0, 1.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    QVERIFY(!router.active());

    resolved = chipHit(HybridChrome::IconChipHitKind::Body);
    // AGENT-NOTE: the exact dock chord must reach the InteractionController.
    const auto modified = router.pointerPress(
        pointer({1.0, 1.0}, Qt::LeftButton, Qt::LeftButton,
                Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(!modified.consumed);
    QVERIFY(modified.raiseRequests.isEmpty());
    QVERIFY(!router.active());

    QVERIFY(!router.pointerPress(pointer({1.0, 1.0}, Qt::MiddleButton, Qt::MiddleButton))
                 .consumed);
    QVERIFY(!hasIconChipDecisionOutput(IconChipPointerDecision{.consumed = true}));
}

void HybridIconChipRouterTests::raisesOnBodyPressAndUnrollsOnDoubleClickOnly()
{
    const auto body = chipHit(HybridChrome::IconChipHitKind::Body);
    HybridIconChipRouter router([body](const QPointF &) { return std::optional(body); },
                                8.0, 400.0);

    const auto hover = router.pointerMove(pointer({10.0, 10.0}));
    QVERIFY(hover.hoverChanged);
    QCOMPARE(hover.hovered, std::optional(body));
    QVERIFY(!hover.consumed);

    const auto press = router.pointerPress(pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton));
    QVERIFY(press.consumed);
    QVERIFY(!press.hoverChanged);
    QCOMPARE(press.raiseRequests, QVector<QString>{QStringLiteral("window-a")});
    QVERIFY(router.active());
    const auto release = router.pointerRelease(pointer({10.0, 10.0}, Qt::LeftButton));
    QVERIFY(release.consumed);
    QVERIFY(release.unrollRequests.isEmpty());
    QVERIFY(release.closeRequests.isEmpty());
    QVERIFY(!router.active());

    // Second click inside the interval: the unroll request rides the release.
    QVERIFY(router.pointerPress(pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    const auto second = router.pointerRelease(pointer({10.0, 10.0}, Qt::LeftButton));
    QCOMPARE(second.unrollRequests, QVector<QString>{QStringLiteral("window-a")});
    QVERIFY(second.drags.isEmpty());

    // A third click is a fresh single click again.
    QVERIFY(router.pointerPress(pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    QVERIFY(router.pointerRelease(pointer({10.0, 10.0}, Qt::LeftButton)).unrollRequests.isEmpty());
}

void HybridIconChipRouterTests::closesOnAMatchingCloseRelease()
{
    std::optional<IconChipPointerHit> resolved = chipHit(HybridChrome::IconChipHitKind::Close);
    HybridIconChipRouter router([&](const QPointF &) { return resolved; });

    const auto press = router.pointerPress(pointer({40.0, 4.0}, Qt::LeftButton, Qt::LeftButton));
    QVERIFY(press.consumed);
    QVERIFY(press.raiseRequests.isEmpty());
    // Dragging from the close glyph never moves the chip.
    const auto move = router.pointerMove(pointer({80.0, 60.0}, Qt::NoButton, Qt::LeftButton));
    QVERIFY(move.consumed);
    QVERIFY(move.drags.isEmpty());
    const auto release = router.pointerRelease(pointer({40.0, 4.0}, Qt::LeftButton));
    QCOMPARE(release.closeRequests, QVector<QString>{QStringLiteral("window-a")});
    QVERIFY(release.unrollRequests.isEmpty());

    // Releasing elsewhere is not a close.
    QVERIFY(router.pointerPress(pointer({40.0, 4.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    resolved = std::nullopt;
    QVERIFY(router.pointerRelease(pointer({200.0, 200.0}, Qt::LeftButton)).closeRequests.isEmpty());
}

void HybridIconChipRouterTests::emitsThresholdedCumulativeDragLifecycle()
{
    const auto body = chipHit(HybridChrome::IconChipHitKind::Body);
    HybridIconChipRouter router([body](const QPointF &) { return std::optional(body); }, 8.0);

    QVERIFY(router.pointerPress(pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    const auto belowThreshold = router.pointerMove(pointer({14.0, 10.0}, Qt::NoButton, Qt::LeftButton));
    QVERIFY(belowThreshold.consumed);
    QVERIFY(belowThreshold.drags.isEmpty());
    const auto begin = router.pointerMove(pointer({30.0, 10.0}, Qt::NoButton, Qt::LeftButton));
    QCOMPARE(begin.drags.size(), 1);
    QCOMPARE(begin.drags.first().phase, HybridChrome::DragPhase::Begin);
    QCOMPARE(begin.drags.first().delta, QPointF(20.0, 0.0));
    const auto update = router.pointerMove(pointer({35.0, 22.0}, Qt::NoButton, Qt::LeftButton));
    QCOMPARE(update.drags.first().phase, HybridChrome::DragPhase::Update);
    QCOMPARE(update.drags.first().delta, QPointF(25.0, 12.0));
    QVERIFY(router.pointerMove(pointer({35.0, 22.0}, Qt::NoButton, Qt::LeftButton)).drags.isEmpty());
    const auto commit = router.pointerRelease(pointer({36.0, 22.0}, Qt::LeftButton));
    QCOMPARE(commit.drags.size(), 1);
    QCOMPARE(commit.drags.first().phase, HybridChrome::DragPhase::Commit);
    QCOMPARE(commit.drags.first().delta, QPointF(26.0, 12.0));
    // A drag release never doubles as a click.
    QVERIFY(commit.unrollRequests.isEmpty());
    QVERIFY(!router.active());

    // A lost release cancels the drag.
    QVERIFY(router.pointerPress(pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    QVERIFY(!router.pointerMove(pointer({40.0, 10.0}, Qt::NoButton, Qt::LeftButton)).drags.isEmpty());
    const auto lost = router.pointerMove(pointer({50.0, 10.0}));
    QVERIFY(lost.consumed);
    QCOMPARE(lost.drags.first().phase, HybridChrome::DragPhase::Cancel);
    QVERIFY(!router.active());
}

void HybridIconChipRouterTests::routesContextMenuOnMatchingRightRelease()
{
    const auto body = chipHit(HybridChrome::IconChipHitKind::Body);
    HybridIconChipRouter router([body](const QPointF &) { return std::optional(body); });
    const auto press = router.pointerPress(pointer({10.0, 10.0}, Qt::RightButton, Qt::RightButton));
    QVERIFY(press.consumed);
    QVERIFY(press.raiseRequests.isEmpty());
    QVERIFY(router.pointerMove(pointer({60.0, 60.0}, Qt::NoButton, Qt::RightButton)).drags.isEmpty());
    const auto release = router.pointerRelease(pointer({12.0, 12.0}, Qt::RightButton));
    QCOMPARE(release.contextMenus,
             (QVector<IconChipContextMenuRequest>{{QStringLiteral("window-a"), QPointF(12.0, 12.0)}}));
    QVERIFY(!router.active());
}

void HybridIconChipRouterTests::cancelsOwnedGrabAndClearsHoverOutsideChips()
{
    std::optional<IconChipPointerHit> resolved = chipHit(HybridChrome::IconChipHitKind::Body);
    HybridIconChipRouter router([&](const QPointF &) { return resolved; }, 8.0);
    QVERIFY(router.pointerPress(pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    QVERIFY(!router.pointerMove(pointer({40.0, 10.0}, Qt::NoButton, Qt::LeftButton)).drags.isEmpty());
    const auto cancelled = router.cancel();
    QVERIFY(cancelled.consumed);
    QCOMPARE(cancelled.drags.first().phase, HybridChrome::DragPhase::Cancel);
    QVERIFY(!router.active());
    QVERIFY(!router.cancel().consumed);

    // cancel() keeps hover (the pointer is still over the chip); only an
    // invalidation forgets it.
    QVERIFY(!router.pointerMove(pointer({10.0, 10.0})).hoverChanged);
    const auto invalidated = router.invalidateTargets();
    QVERIFY(invalidated.hoverChanged);
    QVERIFY(!invalidated.hovered.has_value());
    resolved = std::nullopt;
    QVERIFY(!router.pointerMove(pointer({300.0, 300.0})).hoverChanged);
}

void HybridIconChipRouterTests::wheelTowardTheUserUnrollsAndAwayIsSwallowed()
{
    std::optional<IconChipPointerHit> resolved = chipHit(HybridChrome::IconChipHitKind::Body);
    HybridIconChipRouter router([&](const QPointF &) { return resolved; });

    const auto toward = router.pointerWheel({5.0, 5.0}, Qt::NoModifier, -120.0);
    QVERIFY(toward.consumed);
    QCOMPARE(toward.unrollRequests, QVector<QString>{QStringLiteral("window-a")});
    const auto away = router.pointerWheel({5.0, 5.0}, Qt::NoModifier, 120.0);
    QVERIFY(away.consumed);
    QVERIFY(away.unrollRequests.isEmpty());
    QVERIFY(!router.pointerWheel({5.0, 5.0}, Qt::ControlModifier, -120.0).consumed);
    QVERIFY(!router.pointerWheel({5.0, 5.0}, Qt::NoModifier, 0.0).consumed);
    resolved = std::nullopt;
    QVERIFY(!router.pointerWheel({500.0, 500.0}, Qt::NoModifier, -120.0).consumed);
    // A held grab never rolls.
    resolved = chipHit(HybridChrome::IconChipHitKind::Body);
    QVERIFY(router.pointerPress(pointer({5.0, 5.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    QVERIFY(!router.pointerWheel({5.0, 5.0}, Qt::NoModifier, -120.0).consumed);
}

QTEST_APPLESS_MAIN(HybridIconChipRouterTests)
#include "tst_hybridiconchiprouter.moc"
