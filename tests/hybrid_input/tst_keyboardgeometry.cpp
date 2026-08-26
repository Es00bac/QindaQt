// SPDX-License-Identifier: GPL-3.0-or-later
#include "interactiontestsupport.h"

#include <QTest>

#include <limits>

using namespace QindaQt::HybridInput;
using namespace QindaQt::HybridInput::TestSupport;

class KeyboardGeometryTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void moveUsesConfigurableCumulativeStepAndAutoRepeat();
    void dividerResizeCancelsWithCumulativeDelta();
    void outerEdgeAndCornerResizeFilterAxes();
    void enterCommitsEveryGeometryModeWithoutMovement();
    void invalidSourcesNeverAcquireInput();
    void invalidKeyboardStepsUseDeterministicFallback();
    void unrelatedKeysAndReleasesPassThroughInEveryMode();
};

void KeyboardGeometryTest::moveUsesConfigurableCumulativeStepAndAutoRepeat()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::OuterTitle, QStringLiteral("group"), {}, {}};
    InteractionController controller(resolver, {.keyboardStep = 6.5});

    const auto begin = controller.beginKeyboardMove(source);
    QVERIFY(begin.consumed);
    QCOMPARE(begin.intents.constFirst().kind, InteractionKind::ContainerMove);
    QCOMPARE(begin.intents.constFirst().phase, IntentPhase::Begin);
    QCOMPARE(begin.intents.constFirst().delta, QPointF{});

    const auto right = controller.keyEvent({.key = Qt::Key_Right});
    QCOMPARE(right.intents.constFirst().delta, QPointF(6.5, 0.0));
    const auto repeated = controller.keyEvent(
        {.key = Qt::Key_Right, .pressed = true, .autoRepeat = true});
    QCOMPARE(repeated.intents.constFirst().delta, QPointF(13.0, 0.0));
    const auto up = controller.keyEvent(
        {.key = Qt::Key_Up, .pressed = true, .autoRepeat = true});
    QCOMPARE(up.intents.constFirst().delta, QPointF(13.0, -6.5));
    QCOMPARE(up.intents.constFirst().phase, IntentPhase::Update);

    const auto commit = controller.keyEvent({.key = Qt::Key_Enter});
    QVERIFY(commit.consumed);
    QCOMPARE(commit.intents.constFirst().kind, InteractionKind::ContainerMove);
    QCOMPARE(commit.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(commit.intents.constFirst().delta, QPointF(13.0, -6.5));
    QVERIFY(!controller.active());
}

void KeyboardGeometryTest::dividerResizeCancelsWithCumulativeDelta()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::Divider, QStringLiteral("group"), {},
                           QStringLiteral("split")};
    InteractionController controller(resolver, {.keyboardStep = 4.0});

    const auto begin = controller.beginKeyboardDividerResize(source);
    QVERIFY(begin.consumed);
    QCOMPARE(begin.intents.constFirst().kind, InteractionKind::DividerResize);
    QCOMPARE(controller.keyEvent({.key = Qt::Key_Left}).intents.constFirst().delta,
             QPointF(-4, 0));
    QCOMPARE(controller.keyEvent({.key = Qt::Key_Down}).intents.constFirst().delta,
             QPointF(-4, 4));

    const auto escaped = controller.keyEvent({.key = Qt::Key_Escape});
    QVERIFY(escaped.consumed);
    QCOMPARE(escaped.intents.constFirst().phase, IntentPhase::Cancel);
    QCOMPARE(escaped.intents.constFirst().delta, QPointF(-4, 4));
    QVERIFY(!controller.active());

    QVERIFY(controller.beginKeyboardDividerResize(source).consumed);
    QCOMPARE(controller.keyEvent({.key = Qt::Key_Right}).intents.constFirst().delta,
             QPointF(4, 0));
    const auto cancelled = controller.cancel();
    QCOMPARE(cancelled.intents.constFirst().phase, IntentPhase::Cancel);
    QCOMPARE(cancelled.intents.constFirst().delta, QPointF(4, 0));
}

void KeyboardGeometryTest::outerEdgeAndCornerResizeFilterAxes()
{
    RecordingResolver resolver;
    InteractionController controller(resolver, {.keyboardStep = 3.0});
    const HitTarget leftEdge{HitKind::OuterResize, QStringLiteral("group"), {}, {},
                             Qt::LeftEdge};

    const auto beginEdge = controller.beginKeyboardContainerResize(leftEdge);
    QVERIFY(beginEdge.consumed);
    QCOMPARE(beginEdge.intents.constFirst().kind, InteractionKind::ContainerResize);
    QVERIFY(!controller.keyEvent({.key = Qt::Key_Up}).consumed);
    QVERIFY(controller.active());
    const auto edgeUpdate = controller.keyEvent({.key = Qt::Key_Right});
    QVERIFY(edgeUpdate.consumed);
    QCOMPARE(edgeUpdate.intents.constFirst().delta, QPointF(3, 0));
    const auto edgeCommit = controller.keyEvent({.key = Qt::Key_Return});
    QCOMPARE(edgeCommit.intents.constFirst().delta, QPointF(3, 0));

    const HitTarget corner{HitKind::OuterResize, QStringLiteral("group"), {}, {},
                           Qt::RightEdge | Qt::BottomEdge};
    QVERIFY(controller.beginKeyboardContainerResize(corner).consumed);
    QCOMPARE(controller.keyEvent({.key = Qt::Key_Left}).intents.constFirst().delta,
             QPointF(-3, 0));
    QCOMPARE(controller.keyEvent({.key = Qt::Key_Down}).intents.constFirst().delta,
             QPointF(-3, 3));
    const auto cornerCommit = controller.keyEvent({.key = Qt::Key_Enter});
    QCOMPARE(cornerCommit.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(cornerCommit.intents.constFirst().delta, QPointF(-3, 3));
}

void KeyboardGeometryTest::enterCommitsEveryGeometryModeWithoutMovement()
{
    RecordingResolver resolver;
    InteractionController controller(resolver);

    const HitTarget move{HitKind::OuterTitle, QStringLiteral("group"), {}, {}};
    QVERIFY(controller.beginKeyboardMove(move).consumed);
    auto committed = controller.keyEvent({.key = Qt::Key_Enter});
    QCOMPARE(committed.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(committed.intents.constFirst().delta, QPointF{});

    const HitTarget divider{HitKind::Divider, QStringLiteral("group"), {},
                            QStringLiteral("split")};
    QVERIFY(controller.beginKeyboardDividerResize(divider).consumed);
    committed = controller.keyEvent({.key = Qt::Key_Return});
    QCOMPARE(committed.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(committed.intents.constFirst().delta, QPointF{});

    const HitTarget resize{HitKind::OuterResize, QStringLiteral("group"), {}, {},
                           Qt::TopEdge};
    QVERIFY(controller.beginKeyboardContainerResize(resize).consumed);
    committed = controller.keyEvent({.key = Qt::Key_Enter});
    QCOMPARE(committed.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(committed.intents.constFirst().delta, QPointF{});
}

void KeyboardGeometryTest::invalidSourcesNeverAcquireInput()
{
    RecordingResolver resolver;
    InteractionController controller(resolver);

    QVERIFY(!controller.beginKeyboardMove({}).consumed);
    QVERIFY(!controller.beginKeyboardMove(
        {HitKind::OuterTitle, {}, {}, {}}).consumed);
    QVERIFY(!controller.beginKeyboardMove(
        {HitKind::Divider, QStringLiteral("group"), {}, QStringLiteral("split")})
                 .consumed);

    QVERIFY(!controller.beginKeyboardDividerResize({}).consumed);
    QVERIFY(!controller.beginKeyboardDividerResize(
        {HitKind::Divider, QStringLiteral("group"), {}, {}}).consumed);
    QVERIFY(!controller.beginKeyboardDividerResize(
        {HitKind::OuterTitle, QStringLiteral("group"), {}, {}}).consumed);

    const QVector<HitTarget> invalidResize{
        {},
        {HitKind::OuterResize, {}, {}, {}, Qt::LeftEdge},
        {HitKind::OuterResize, QStringLiteral("group"), {}, {}, {}},
        {HitKind::OuterResize, QStringLiteral("group"), {}, {},
         Qt::LeftEdge | Qt::RightEdge},
        {HitKind::OuterResize, QStringLiteral("group"), {}, {},
         Qt::TopEdge | Qt::BottomEdge},
        {HitKind::OuterTitle, QStringLiteral("group"), {}, {}},
    };
    for (const auto &source : invalidResize) {
        QVERIFY(!controller.beginKeyboardContainerResize(source).consumed);
        QVERIFY(!controller.active());
    }

    const HitTarget validMove{HitKind::OuterTitle, QStringLiteral("group"), {}, {}};
    QVERIFY(controller.beginKeyboardMove(validMove).consumed);
    QVERIFY(!controller.beginKeyboardDividerResize(
        {HitKind::Divider, QStringLiteral("group"), {}, QStringLiteral("split")})
                 .consumed);
    QCOMPARE(controller.interactionKind(), InteractionKind::ContainerMove);
}

void KeyboardGeometryTest::invalidKeyboardStepsUseDeterministicFallback()
{
    const QVector<qreal> invalid{
        0.0,
        -2.0,
        std::numeric_limits<qreal>::infinity(),
        std::numeric_limits<qreal>::quiet_NaN(),
    };
    for (qreal step : invalid) {
        RecordingResolver resolver;
        InteractionController controller(resolver, {.keyboardStep = step});
        QVERIFY(controller.beginKeyboardMove(
            {HitKind::OuterTitle, QStringLiteral("group"), {}, {}}).consumed);
        const auto update = controller.keyEvent({.key = Qt::Key_Down});
        QCOMPARE(update.intents.constFirst().delta, QPointF(0, 10));
    }
}

void KeyboardGeometryTest::unrelatedKeysAndReleasesPassThroughInEveryMode()
{
    RecordingResolver resolver;
    InteractionController controller(resolver);
    const QVector<HitTarget> sources{
        {HitKind::OuterTitle, QStringLiteral("group"), {}, {}},
        {HitKind::Divider, QStringLiteral("group"), {}, QStringLiteral("split")},
        {HitKind::OuterResize, QStringLiteral("group"), {}, {}, Qt::BottomEdge},
    };

    for (qsizetype index = 0; index < sources.size(); ++index) {
        InteractionDecision begin;
        if (index == 0) {
            begin = controller.beginKeyboardMove(sources[index]);
        } else if (index == 1) {
            begin = controller.beginKeyboardDividerResize(sources[index]);
        } else {
            begin = controller.beginKeyboardContainerResize(sources[index]);
        }
        QVERIFY(begin.consumed);
        QVERIFY(!controller.keyEvent({.key = Qt::Key_Q}).consumed);
        QVERIFY(!controller.keyEvent({.key = Qt::Key_Down, .pressed = false}).consumed);
        if (index == 2) {
            QVERIFY(!controller.keyEvent({.key = Qt::Key_Left}).consumed);
        }
        QVERIFY(controller.active());
        QVERIFY(controller.keyEvent({.key = Qt::Key_Escape}).consumed);
    }
}

QTEST_GUILESS_MAIN(KeyboardGeometryTest)
#include "tst_keyboardgeometry.moc"
