// SPDX-License-Identifier: GPL-3.0-or-later
#include "interactiontestsupport.h"

#include <QTest>

using namespace QindaQt::HybridInput;
using namespace QindaQt::HybridInput::TestSupport;

class KeyboardDockTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directionalAndTabSelectionsCommitResolvedTargets();
    void sameContainerTargetSupportsReordering();
    void groupedMemberCanSelectExplicitDetach();
    void selectionCanMoveBetweenDockAndDetach();
    void independentMemberCannotSelectDetach();
    void missingDockTargetCancelsOnEnter();
    void escapeAndExternalCancelEndTheMode();
    void invalidSourcesNeverAcquireInput();
    void unrelatedKeysAndReleasesPassThrough();
};

void KeyboardDockTest::directionalAndTabSelectionsCommitResolvedTargets()
{
    struct Selection {
        Qt::Key key;
        DockZone zone;
    };
    const QVector<Selection> selections{
        {Qt::Key_Left, DockZone::Left},
        {Qt::Key_Right, DockZone::Right},
        {Qt::Key_Up, DockZone::Top},
        {Qt::Key_Down, DockZone::Bottom},
        {Qt::Key_T, DockZone::Tab},
    };

    for (const auto &selection : selections) {
        RecordingResolver resolver;
        const HitTarget source{HitKind::Tab, QStringLiteral("source-group"),
                               QStringLiteral("source-window"), {}};
        const DockTarget target{QStringLiteral("target-group"),
                                QStringLiteral("target-window"), selection.zone};
        resolver.keyboardTargets.insert(selection.zone, target);
        InteractionController controller(resolver);

        const auto begin = controller.beginKeyboardDock(source);
        QVERIFY(begin.consumed);
        QCOMPARE(begin.intents.size(), 1);
        QCOMPARE(begin.intents.constFirst().kind, InteractionKind::MemberDock);
        QCOMPARE(begin.intents.constFirst().phase, IntentPhase::Begin);
        QCOMPARE(begin.intents.constFirst().source, source);
        QCOMPARE(begin.intents.constFirst().delta, QPointF{});

        const auto preview = controller.keyEvent(
            {.key = selection.key, .pressed = true, .autoRepeat = true});
        QVERIFY(preview.consumed);
        QCOMPARE(preview.intents.size(), 1);
        QCOMPARE(preview.intents.constFirst().phase, IntentPhase::Update);
        QCOMPARE(preview.intents.constFirst().target, target);
        QCOMPARE(resolver.keyboardQueries, QVector<DockZone>{selection.zone});

        const auto commit = controller.keyEvent({.key = Qt::Key_Return});
        QVERIFY(commit.consumed);
        QCOMPARE(commit.intents.size(), 1);
        QCOMPARE(commit.intents.constFirst().phase, IntentPhase::Commit);
        QCOMPARE(commit.intents.constFirst().target, target);
        QVERIFY(!controller.active());
    }
}

void KeyboardDockTest::sameContainerTargetSupportsReordering()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::Tab, QStringLiteral("group"),
                           QStringLiteral("window-b"), {}};
    const DockTarget earlierMember{QStringLiteral("group"),
                                   QStringLiteral("window-a"), DockZone::Tab};
    resolver.keyboardTargets.insert(DockZone::Tab, earlierMember);
    InteractionController controller(resolver);

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    QCOMPARE(controller.keyEvent({.key = Qt::Key_T}).intents.constFirst().target,
             earlierMember);
    const auto commit = controller.keyEvent({.key = Qt::Key_Enter});
    QCOMPARE(commit.intents.constFirst().phase, IntentPhase::Commit);
    QCOMPARE(commit.intents.constFirst().target, earlierMember);
}

void KeyboardDockTest::groupedMemberCanSelectExplicitDetach()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::MemberTitle, QStringLiteral("group"),
                           QStringLiteral("window"), {}};
    InteractionController controller(resolver);

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    const auto selected = controller.keyEvent({.key = Qt::Key_D});
    QVERIFY(selected.consumed);
    QCOMPARE(selected.intents.size(), 1);
    QCOMPARE(selected.intents.constFirst().phase, IntentPhase::Update);
    QVERIFY(!selected.intents.constFirst().target.isValid());

    const auto commit = controller.keyEvent({.key = Qt::Key_Enter});
    QVERIFY(commit.consumed);
    QCOMPARE(commit.intents.constFirst().phase, IntentPhase::Commit);
    QVERIFY(!commit.intents.constFirst().target.isValid());
    QVERIFY(!controller.active());
}

void KeyboardDockTest::selectionCanMoveBetweenDockAndDetach()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::Tab, QStringLiteral("group-a"),
                           QStringLiteral("window-a"), {}};
    const DockTarget left{QStringLiteral("group-b"), QStringLiteral("window-b"),
                          DockZone::Left};
    resolver.keyboardTargets.insert(DockZone::Left, left);
    InteractionController controller(resolver);

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    QVERIFY(controller.keyEvent({.key = Qt::Key_D}).consumed);
    QCOMPARE(controller.keyEvent({.key = Qt::Key_Left}).intents.constFirst().target,
             left);
    QCOMPARE(controller.keyEvent({.key = Qt::Key_Enter}).intents.constFirst().target,
             left);

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    QVERIFY(controller.keyEvent({.key = Qt::Key_Left}).consumed);
    QVERIFY(controller.keyEvent({.key = Qt::Key_D}).consumed);
    const auto detached = controller.keyEvent({.key = Qt::Key_Enter});
    QCOMPARE(detached.intents.constFirst().phase, IntentPhase::Commit);
    QVERIFY(!detached.intents.constFirst().target.isValid());
}

void KeyboardDockTest::independentMemberCannotSelectDetach()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::MemberTitle, {}, QStringLiteral("window"), {}};
    InteractionController controller(resolver);

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    const auto rejected = controller.keyEvent({.key = Qt::Key_D});
    QVERIFY(rejected.consumed);
    QVERIFY(rejected.intents.isEmpty());
    QVERIFY(controller.active());

    const auto enter = controller.keyEvent({.key = Qt::Key_Enter});
    QVERIFY(enter.consumed);
    QCOMPARE(enter.intents.constFirst().phase, IntentPhase::Cancel);
    QVERIFY(!enter.intents.constFirst().target.isValid());
    QVERIFY(!controller.active());
}

void KeyboardDockTest::missingDockTargetCancelsOnEnter()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::MemberTitle, {}, QStringLiteral("window"), {}};
    InteractionController controller(resolver);

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    const auto preview = controller.keyEvent({.key = Qt::Key_Right});
    QVERIFY(preview.consumed);
    QCOMPARE(preview.intents.constFirst().phase, IntentPhase::Update);
    QVERIFY(!preview.intents.constFirst().target.isValid());
    const auto commit = controller.keyEvent({.key = Qt::Key_Return});
    QCOMPARE(commit.intents.constFirst().phase, IntentPhase::Cancel);
}

void KeyboardDockTest::escapeAndExternalCancelEndTheMode()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::Tab, QStringLiteral("group"),
                           QStringLiteral("window"), {}};
    InteractionController controller(resolver);

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    const auto escaped = controller.keyEvent({.key = Qt::Key_Escape});
    QVERIFY(escaped.consumed);
    QCOMPARE(escaped.intents.constFirst().phase, IntentPhase::Cancel);
    QVERIFY(!controller.active());

    QVERIFY(controller.beginKeyboardDock(source).consumed);
    const auto cancelled = controller.cancel();
    QVERIFY(cancelled.consumed);
    QCOMPARE(cancelled.intents.constFirst().phase, IntentPhase::Cancel);
    QVERIFY(!controller.active());
    QVERIFY(!controller.cancel().consumed);
}

void KeyboardDockTest::invalidSourcesNeverAcquireInput()
{
    RecordingResolver resolver;
    InteractionController controller(resolver);
    const QVector<HitTarget> invalid{
        {},
        {HitKind::MemberTitle, {}, {}, {}},
        {HitKind::Tab, QStringLiteral("group"), {}, {}},
        {HitKind::OuterTitle, QStringLiteral("group"), {}, {}},
        {HitKind::Divider, QStringLiteral("group"), {}, QStringLiteral("split")},
        {HitKind::OuterResize, QStringLiteral("group"), {}, {}, Qt::LeftEdge},
    };
    for (const auto &source : invalid) {
        QVERIFY(!controller.beginKeyboardDock(source).consumed);
        QVERIFY(!controller.active());
        QVERIFY(!controller.keyEvent({.key = Qt::Key_Left}).consumed);
    }

    const HitTarget valid{HitKind::MemberTitle, {}, QStringLiteral("window"), {}};
    QVERIFY(controller.beginKeyboardDock(valid).consumed);
    QVERIFY(!controller.beginKeyboardDock(valid).consumed);
    QVERIFY(controller.active());
}

void KeyboardDockTest::unrelatedKeysAndReleasesPassThrough()
{
    RecordingResolver resolver;
    const HitTarget source{HitKind::MemberTitle, {}, QStringLiteral("window"), {}};
    InteractionController controller(resolver);

    QVERIFY(!controller.keyEvent({.key = Qt::Key_Q}).consumed);
    QVERIFY(controller.beginKeyboardDock(source).consumed);
    QVERIFY(!controller.keyEvent({.key = Qt::Key_Q}).consumed);
    QVERIFY(!controller.keyEvent({.key = Qt::Key_Left, .pressed = false}).consumed);
    QVERIFY(controller.active());
    const auto preview = controller.keyEvent({.key = Qt::Key_Left});
    QVERIFY(preview.consumed);
    QVERIFY(controller.active());
}

QTEST_GUILESS_MAIN(KeyboardDockTest)
#include "tst_keyboarddock.moc"
