// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_customization_editor/gesture_state_machine.h"

#include <QtTest/QtTest>

using namespace QindaQt::ShellCustomizationEditor;

namespace {

GestureEvent armEvent()
{
    return {GestureEventKind::Arm, false, {}};
}

GestureEvent thresholdEvent()
{
    return {GestureEventKind::ThresholdExceeded, false, {}};
}

GestureEvent hoverEvent(const DropTarget &target)
{
    return {GestureEventKind::HoverChanged, false, target};
}

GestureEvent settledEvent(GestureEventKind kind, bool ok)
{
    return {kind, ok, {}};
}

GestureEvent gestureEventFor(GestureEventKind kind)
{
    return {kind, false, {}};
}

void advance(GestureStateMachine &machine, const GestureEvent &input)
{
    const GestureTransition transition = machine.handle(input);
    Q_UNUSED(transition);
}

bool hasDirective(const GestureTransition &transition, GestureDirectiveKind kind)
{
    for (const GestureDirective &directive : transition.directives) {
        if (directive.kind == kind) {
            return true;
        }
    }
    return false;
}

} // namespace

class GestureStateMachineTest final : public QObject
{
    Q_OBJECT

private slots:
    void noVisualDragWithoutOpenedPreview() const
    {
        // Invariant 1: Dragging is entered on threshold, but the visual drag
        // exists only after BeginPreview succeeded.
        GestureStateMachine machine;
        QVERIFY(machine.handle(armEvent()).directives.isEmpty());

        const GestureTransition threshold = machine.handle(thresholdEvent());
        QCOMPARE(threshold.state, GestureState::Dragging);
        QVERIFY(hasDirective(threshold, GestureDirectiveKind::BeginPreview));

        const GestureTransition failed = machine.handle(settledEvent(GestureEventKind::PreviewSettled, false));
        QCOMPARE(failed.state, GestureState::Idle);
        QVERIFY(!failed.refused);
        QCOMPARE(machine.state(), GestureState::Idle);
    }

    void openPreviewKeepsDraggingAndResetsOnDrop()
    {
        GestureStateMachine machine;
        advance(machine, armEvent());
        advance(machine, thresholdEvent());
        const GestureTransition opened =
            machine.handle(settledEvent(GestureEventKind::PreviewSettled, true));
        QCOMPARE(opened.state, GestureState::Dragging);
        QVERIFY(opened.directives.isEmpty());

        const GestureTransition drop =
            machine.handle(gestureEventFor(GestureEventKind::Drop));
        QCOMPARE(drop.state, GestureState::Committing);
        QVERIFY(hasDirective(drop, GestureDirectiveKind::CommitPreview));

        const GestureTransition committed =
            machine.handle(settledEvent(GestureEventKind::CommitSettled, true));
        QCOMPARE(committed.state, GestureState::Idle);
        QVERIFY(!machine.resolvedTarget().has_value());
    }

    void cancelAlwaysClosesThroughCancelling()
    {
        // Invariant 2: Escape reaches Cancelling from Dragging, and the
        // CancelSettled transition is unconditional.
        GestureStateMachine machine;
        advance(machine, armEvent());
        advance(machine, thresholdEvent());
        advance(machine, settledEvent(GestureEventKind::PreviewSettled, true));

        const GestureTransition cancel =
            machine.handle(gestureEventFor(GestureEventKind::CancelRequested));
        QCOMPARE(cancel.state, GestureState::Cancelling);
        QVERIFY(hasDirective(cancel, GestureDirectiveKind::CancelPreview));

        const GestureTransition settled =
            machine.handle(settledEvent(GestureEventKind::CancelSettled, false));
        QCOMPARE(settled.state, GestureState::Idle);
    }

    void armClickWithoutThresholdReturnsToIdle()
    {
        GestureStateMachine machine;
        advance(machine, armEvent());
        const GestureTransition click =
            machine.handle(gestureEventFor(GestureEventKind::Drop));
        QCOMPARE(click.state, GestureState::Idle);
        QVERIFY(click.directives.isEmpty());
    }

    void refusedEventsKeepTheState() const
    {
        GestureStateMachine machine;
        const GestureTransition refused = machine.handle(thresholdEvent());
        QVERIFY(refused.refused);
        QCOMPARE(refused.state, GestureState::Idle);
        QVERIFY(!refused.reason.isEmpty());
    }

    void outputGenerationChangeClosesAnOpenGesture()
    {
        // Invariant 6: an output-generation change in any non-idle state with
        // an open preview forces the cancel path.
        GestureStateMachine machine;
        advance(machine, armEvent());
        advance(machine, thresholdEvent());
        advance(machine, settledEvent(GestureEventKind::PreviewSettled, true));

        const GestureTransition changed =
            machine.handle(gestureEventFor(GestureEventKind::OutputGenerationChanged));
        QCOMPARE(changed.state, GestureState::Cancelling);
        QVERIFY(hasDirective(changed, GestureDirectiveKind::CancelPreview));

        // Arming has no preview: the change lands straight in Idle.
        GestureStateMachine armed;
        advance(armed, armEvent());
        const GestureTransition armedChanged =
            armed.handle(gestureEventFor(GestureEventKind::OutputGenerationChanged));
        QCOMPARE(armedChanged.state, GestureState::Idle);
        QVERIFY(armedChanged.directives.isEmpty());
    }

    void hoverExecutesOnlyWhenTargetIdentityChanges() const
    {
        // Architecture D3: evaluate per hover change, execute per resolved
        // target identity change — never per pointer-motion event.
        GestureStateMachine machine;
        advance(machine, armEvent());
        advance(machine, thresholdEvent());
        advance(machine, settledEvent(GestureEventKind::PreviewSettled, true));

        const DropTarget first{QStringLiteral("bar"), QStringLiteral("start"), {}};
        const DropTarget sameAsFirst{QStringLiteral("bar"), QStringLiteral("start"), {}};
        const DropTarget second{QStringLiteral("bar"), QStringLiteral("end"), {}};

        const GestureTransition firstHover = machine.handle(hoverEvent(first));
        QVERIFY(hasDirective(firstHover, GestureDirectiveKind::Evaluate));
        QVERIFY(hasDirective(firstHover, GestureDirectiveKind::ExecutePending));
        QVERIFY(machine.resolvedTarget().has_value());

        const GestureTransition repeatedHover = machine.handle(hoverEvent(sameAsFirst));
        QVERIFY(!hasDirective(repeatedHover, GestureDirectiveKind::Evaluate));
        QVERIFY(!hasDirective(repeatedHover, GestureDirectiveKind::ExecutePending));

        const GestureTransition changedHover = machine.handle(hoverEvent(second));
        QVERIFY(hasDirective(changedHover, GestureDirectiveKind::ExecutePending));
        QCOMPARE(machine.resolvedTarget()->zone, QStringLiteral("end"));
    }

    void failedCommitDemandsARollback()
    {
        GestureStateMachine machine;
        advance(machine, armEvent());
        advance(machine, thresholdEvent());
        advance(machine, settledEvent(GestureEventKind::PreviewSettled, true));
        advance(machine, gestureEventFor(GestureEventKind::Drop));

        const GestureTransition failed =
            machine.handle(settledEvent(GestureEventKind::CommitSettled, false));
        QCOMPARE(failed.state, GestureState::Cancelling);
        QVERIFY(hasDirective(failed, GestureDirectiveKind::CancelPreview));

        const GestureTransition settled =
            machine.handle(settledEvent(GestureEventKind::CancelSettled, true));
        QCOMPARE(settled.state, GestureState::Idle);
    }

    void keyboardAndPointerEventStreamsAreIndistinguishable()
    {
        // Invariant 5 / D7: the keyboard move mode feeds the identical machine
        // with identical events, so identical streams must produce identical
        // state and directive sequences.
        const auto run = [](const QVector<GestureEvent> &events) {
            GestureStateMachine machine;
            QVector<GestureState> states;
            QVector<int> directiveKinds;
            for (const GestureEvent &event : events) {
                const GestureTransition transition = machine.handle(event);
                states.append(transition.state);
                for (const GestureDirective &directive : transition.directives) {
                    directiveKinds.append(static_cast<int>(directive.kind));
                }
            }
            return qMakePair(states, directiveKinds);
        };

        const DropTarget target{QStringLiteral("bar"), QStringLiteral("end"),
                                QStringLiteral("clock-instance")};
        // Construct the two adapter outputs independently. This catches a
        // future pointer/keyboard mapping drift instead of comparing one
        // container with a copy of itself.
        const QVector<GestureEvent> keyboardStream{
            armEvent(),
            thresholdEvent(),
            settledEvent(GestureEventKind::PreviewSettled, true),
            hoverEvent(target),
            gestureEventFor(GestureEventKind::Drop),
            settledEvent(GestureEventKind::CommitSettled, true),
        };
        const QVector<GestureEvent> pointerStream{
            {GestureEventKind::Arm, false, {}},
            {GestureEventKind::ThresholdExceeded, false, {}},
            {GestureEventKind::PreviewSettled, true, {}},
            {GestureEventKind::HoverChanged, false, target},
            {GestureEventKind::Drop, false, {}},
            {GestureEventKind::CommitSettled, true, {}},
        };

        const auto keyboard = run(keyboardStream);
        const auto pointer = run(pointerStream);
        QCOMPARE(keyboard.first, pointer.first);
        QCOMPARE(keyboard.second, pointer.second);
        QCOMPARE(keyboard.first.last(), GestureState::Idle);
    }
};

QTEST_MAIN(GestureStateMachineTest)
#include "tst_gesture_state_machine.moc"
