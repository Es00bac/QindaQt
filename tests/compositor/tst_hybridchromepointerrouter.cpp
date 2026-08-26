// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromepointerrouter.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;

namespace {

ChromePointerHit hit(HybridChrome::HitKind kind,
                     QString stableId = {},
                     qsizetype logicalIndex = -1,
                     std::optional<HybridChrome::WindowAction> action = std::nullopt,
                     Qt::Edges edges = {})
{
    return {QStringLiteral("container-a"),
            {kind, std::move(stableId), logicalIndex, action, edges}};
}

HybridInput::PointerEvent pointer(
    QPointF position,
    Qt::MouseButton changedButton = Qt::NoButton,
    Qt::MouseButtons buttons = {},
    Qt::KeyboardModifiers modifiers = {})
{
    return {position, changedButton, buttons, modifiers};
}

} // namespace

class HybridChromePointerRouterTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void passesNativeMemberClientAndModifierPresses();
    void routesOuterTitleContextMenuOnMatchingRightRelease();
    void reportsRaiseOnlyDecisionAsDispatchable();
    void activatesClicksWithoutAlsoCommittingDrags();
    void emitsThresholdedCumulativeDragLifecycle();
    void cancelsOwnedGrabAndClearsHoverOutsideChrome();
};

void HybridChromePointerRouterTests::reportsRaiseOnlyDecisionAsDispatchable()
{
    const auto outerTitle = hit(HybridChrome::HitKind::OuterTitleDrag,
                                QStringLiteral("container-a"));
    HybridChromePointerRouter router(
        [outerTitle](const QPointF &) { return std::optional(outerTitle); });

    // AGENT-NOTE: Settling hover reproduces the production sequence: the
    // following press has no hover delta and carries only the group raise.
    const auto hover = router.pointerMove(pointer({10.0, 10.0}));
    QVERIFY(hasChromeDecisionOutput(hover));
    const auto press = router.pointerPress(
        pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton));
    QVERIFY(press.consumed);
    QVERIFY(!press.hoverChanged);
    QCOMPARE(press.containerRaiseRequests,
             QVector<QString>{QStringLiteral("container-a")});
    QVERIFY(press.activations.isEmpty());
    QVERIFY(press.drags.isEmpty());
    QVERIFY(hasChromeDecisionOutput(press));

    ChromePointerDecision consumedOnly;
    consumedOnly.consumed = true;
    QVERIFY(!hasChromeDecisionOutput(consumedOnly));
}

void HybridChromePointerRouterTests::passesNativeMemberClientAndModifierPresses()
{
    std::optional<ChromePointerHit> resolved;
    HybridChromePointerRouter router(
        [&](const QPointF &) { return resolved; }, 8.0);

    resolved = hit(HybridChrome::HitKind::MemberTitleDrag,
                   QStringLiteral("window-a"));
    auto decision = router.pointerPress(
        pointer({10.0, 10.0}, Qt::LeftButton, Qt::LeftButton));
    QVERIFY(!decision.consumed);
    QVERIFY(decision.containerRaiseRequests.isEmpty());
    QVERIFY(!router.active());

    resolved = hit(HybridChrome::HitKind::Client);
    decision = router.pointerPress(
        pointer({20.0, 20.0}, Qt::LeftButton, Qt::LeftButton));
    QVERIFY(!decision.consumed);
    QVERIFY(!router.active());

    resolved = hit(HybridChrome::HitKind::OuterTitleDrag,
                   QStringLiteral("container-a"));
    decision = router.pointerPress(
        pointer({30.0, 30.0}, Qt::LeftButton, Qt::LeftButton,
                Qt::MetaModifier | Qt::ShiftModifier));
    QVERIFY(!decision.consumed);
    QVERIFY(decision.containerRaiseRequests.isEmpty());
    QVERIFY(!router.active());

    resolved = hit(HybridChrome::HitKind::MemberTitleDrag,
                   QStringLiteral("window-a"));
    decision = router.pointerPress(
        pointer({30.0, 30.0}, Qt::RightButton, Qt::RightButton));
    QVERIFY(!decision.consumed);
    QVERIFY(!router.active());
}

void HybridChromePointerRouterTests::routesOuterTitleContextMenuOnMatchingRightRelease()
{
    std::optional<ChromePointerHit> resolved =
        hit(HybridChrome::HitKind::OuterTitleDrag,
            QStringLiteral("container-a"));
    HybridChromePointerRouter router(
        [&](const QPointF &) { return resolved; }, 8.0);

    auto decision = router.pointerPress(
        pointer({40.0, 30.0}, Qt::RightButton, Qt::RightButton));
    QVERIFY(decision.consumed);
    QVERIFY(router.active());
    QCOMPARE(decision.containerRaiseRequests,
             QVector<QString>{QStringLiteral("container-a")});
    QVERIFY(decision.contextMenus.isEmpty());

    decision = router.pointerMove(
        pointer({42.0, 31.0}, Qt::NoButton, Qt::RightButton));
    QVERIFY(decision.consumed);
    QVERIFY(decision.drags.isEmpty());
    decision = router.pointerRelease(
        pointer({42.0, 31.0}, Qt::RightButton, {}));
    QVERIFY(decision.consumed);
    const QVector<ChromeContextMenuRequest> expectedRequests{
        {QStringLiteral("container-a"), QPointF(42.0, 31.0)}};
    QCOMPARE(decision.contextMenus, expectedRequests);
    QVERIFY(!router.active());

    decision = router.pointerPress(
        pointer({50.0, 40.0}, Qt::RightButton, Qt::RightButton,
                Qt::MetaModifier));
    QVERIFY(!decision.consumed);

    QVERIFY(router.pointerPress(
        pointer({50.0, 40.0}, Qt::RightButton, Qt::RightButton)).consumed);
    resolved.reset();
    decision = router.pointerRelease(
        pointer({500.0, 400.0}, Qt::RightButton, {}));
    QVERIFY(decision.consumed);
    QVERIFY(decision.contextMenus.isEmpty());
    QVERIFY(!router.active());
}

void HybridChromePointerRouterTests::activatesClicksWithoutAlsoCommittingDrags()
{
    std::optional<ChromePointerHit> resolved =
        hit(HybridChrome::HitKind::Tab, QStringLiteral("page-a"), 2);
    HybridChromePointerRouter router(
        [&](const QPointF &) { return resolved; }, 8.0);

    const auto press = router.pointerPress(
        pointer({0.0, 0.0}, Qt::LeftButton, Qt::LeftButton));
    QVERIFY(press.consumed);
    QCOMPARE(press.containerRaiseRequests,
             QVector<QString>{QStringLiteral("container-a")});
    auto pending = router.pointerMove(pointer({4.0, 0.0}, Qt::NoButton,
                                              Qt::LeftButton));
    QVERIFY(pending.consumed);
    QVERIFY(pending.drags.isEmpty());
    const auto clicked = router.pointerRelease(
        pointer({4.0, 0.0}, Qt::LeftButton, {}));
    QCOMPARE(clicked.activations,
             QVector<ChromePointerHit>({*resolved}));
    QVERIFY(clicked.drags.isEmpty());

    resolved = hit(HybridChrome::HitKind::WindowButton,
                   QStringLiteral("container-a"), -1,
                   HybridChrome::WindowAction::Close);
    QVERIFY(router.pointerPress(
        pointer({10.0, 0.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    const auto buttonMotion = router.pointerMove(
        pointer({100.0, 0.0}, Qt::NoButton, Qt::LeftButton));
    QVERIFY(buttonMotion.drags.isEmpty());
    const auto buttonClick = router.pointerRelease(
        pointer({100.0, 0.0}, Qt::LeftButton, {}));
    QCOMPARE(buttonClick.activations.size(), 1);
    QCOMPARE(buttonClick.activations.constFirst().target.action,
             std::optional(HybridChrome::WindowAction::Close));

    resolved = hit(HybridChrome::HitKind::OuterTitleDrag,
                   QStringLiteral("container-a"));
    QVERIFY(router.pointerPress(
        pointer({0.0, 0.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    const auto outerClick = router.pointerRelease(
        pointer({0.0, 0.0}, Qt::LeftButton, {}));
    QVERIFY(outerClick.activations.isEmpty());
    QVERIFY(outerClick.drags.isEmpty());
}

void HybridChromePointerRouterTests::emitsThresholdedCumulativeDragLifecycle()
{
    const auto tab = hit(HybridChrome::HitKind::Tab,
                         QStringLiteral("page-a"), 0);
    HybridChromePointerRouter router(
        [tab](const QPointF &) { return std::optional(tab); }, 8.0);

    QVERIFY(router.pointerPress(
        pointer({100.0, 50.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    auto decision = router.pointerMove(
        pointer({107.0, 50.0}, Qt::NoButton, Qt::LeftButton));
    QVERIFY(decision.drags.isEmpty());

    decision = router.pointerMove(
        pointer({108.0, 52.0}, Qt::NoButton, Qt::LeftButton));
    QCOMPARE(decision.drags.size(), 1);
    QCOMPARE(decision.drags.constFirst().event.phase,
             HybridChrome::DragPhase::Begin);
    QCOMPARE(decision.drags.constFirst().event.delta, QPointF(8.0, 2.0));

    decision = router.pointerMove(
        pointer({112.0, 55.0}, Qt::NoButton, Qt::LeftButton));
    QCOMPARE(decision.drags.size(), 1);
    QCOMPARE(decision.drags.constFirst().event.phase,
             HybridChrome::DragPhase::Update);
    QCOMPARE(decision.drags.constFirst().event.delta, QPointF(12.0, 5.0));

    decision = router.pointerRelease(
        pointer({115.0, 60.0}, Qt::LeftButton, {}));
    QCOMPARE(decision.drags.size(), 1);
    QCOMPARE(decision.drags.constFirst().event.phase,
             HybridChrome::DragPhase::Commit);
    QCOMPARE(decision.drags.constFirst().event.delta, QPointF(15.0, 10.0));
    QVERIFY(decision.activations.isEmpty());
    QVERIFY(!router.active());
}

void HybridChromePointerRouterTests::cancelsOwnedGrabAndClearsHoverOutsideChrome()
{
    std::optional<ChromePointerHit> resolved =
        hit(HybridChrome::HitKind::Divider, QStringLiteral("split-a"));
    HybridChromePointerRouter router(
        [&](const QPointF &) { return resolved; }, 4.0);

    auto decision = router.pointerMove(pointer({1.0, 1.0}));
    QVERIFY(decision.hoverChanged);
    QCOMPARE(decision.hovered, resolved);
    decision = router.pointerMove(pointer({2.0, 2.0}));
    QVERIFY(!decision.hoverChanged);

    QVERIFY(router.pointerPress(
        pointer({1.0, 1.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    decision = router.pointerMove(
        pointer({10.0, 1.0}, Qt::NoButton, Qt::LeftButton));
    QCOMPARE(decision.drags.constFirst().event.phase,
             HybridChrome::DragPhase::Begin);
    decision = router.cancel();
    QVERIFY(decision.consumed);
    QCOMPARE(decision.drags.constFirst().event.phase,
             HybridChrome::DragPhase::Cancel);
    QVERIFY(!router.active());

    decision = router.invalidateTargets();
    QVERIFY(decision.hoverChanged);
    QVERIFY(!decision.hovered);
    decision = router.pointerMove(pointer({2.0, 2.0}));
    QVERIFY(decision.hoverChanged);
    QCOMPARE(decision.hovered, resolved);

    resolved.reset();
    decision = router.pointerMove(pointer({500.0, 500.0}));
    QVERIFY(decision.hoverChanged);
    QVERIFY(!decision.hovered);

    // A vanished release is also a terminal cancel and cannot leave a grab.
    resolved = hit(HybridChrome::HitKind::OuterResize,
                   QStringLiteral("container-a"), -1, std::nullopt,
                   Qt::RightEdge);
    QVERIFY(router.pointerPress(
        pointer({0.0, 0.0}, Qt::LeftButton, Qt::LeftButton)).consumed);
    static_cast<void>(router.pointerMove(
        pointer({8.0, 0.0}, Qt::NoButton, Qt::LeftButton)));
    decision = router.pointerMove(pointer({9.0, 0.0}));
    QCOMPARE(decision.drags.constLast().event.phase,
             HybridChrome::DragPhase::Cancel);
    QVERIFY(!router.active());
}

QTEST_GUILESS_MAIN(HybridChromePointerRouterTests)
#include "tst_hybridchromepointerrouter.moc"
