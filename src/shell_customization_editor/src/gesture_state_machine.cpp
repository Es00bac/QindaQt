// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/gesture_state_machine.h"

#include <utility>

namespace QindaQt::ShellCustomizationEditor {

QString gestureStateName(GestureState state)
{
    switch (state) {
    case GestureState::Idle:
        return QStringLiteral("idle");
    case GestureState::Arming:
        return QStringLiteral("arming");
    case GestureState::Dragging:
        return QStringLiteral("dragging");
    case GestureState::Committing:
        return QStringLiteral("committing");
    case GestureState::Cancelling:
        return QStringLiteral("cancelling");
    }
    return QStringLiteral("unknown");
}

GestureTransition GestureStateMachine::refuse(GestureEventKind kind)
{
    GestureTransition transition;
    transition.state = m_state;
    transition.refused = true;
    transition.reason =
        QStringLiteral("gesture event %1 is not handled in state %2")
            .arg(static_cast<int>(kind))
            .arg(gestureStateName(m_state));
    return transition;
}

GestureTransition GestureStateMachine::transition(GestureState state,
                                                 QVector<GestureDirective> directives)
{
    m_state = state;
    GestureTransition result;
    result.state = state;
    result.directives = std::move(directives);
    return result;
}

GestureTransition GestureStateMachine::enterCancelling()
{
    GestureDirective cancel;
    cancel.kind = GestureDirectiveKind::CancelPreview;
    return transition(GestureState::Cancelling, {cancel});
}

void GestureStateMachine::reset()
{
    m_state = GestureState::Idle;
    m_resolvedTarget.reset();
}

GestureTransition GestureStateMachine::handle(const GestureEvent &event)
{
    switch (m_state) {
    case GestureState::Idle:
        switch (event.kind) {
        case GestureEventKind::Arm:
            m_resolvedTarget.reset();
            return transition(GestureState::Arming, {});
        case GestureEventKind::OutputGenerationChanged:
            // Nothing is open; the session still rebuilds its inventory.
            return transition(GestureState::Idle, {});
        default:
            return refuse(event.kind);
        }

    case GestureState::Arming:
        switch (event.kind) {
        case GestureEventKind::ThresholdExceeded: {
            // AGENT-GUARD (invariant 1): the visual drag must not appear until
            // the preview actually opened. The state enters Dragging here, but
            // presentation may paint a drag only after PreviewSettled(ok).
            GestureDirective begin;
            begin.kind = GestureDirectiveKind::BeginPreview;
            return transition(GestureState::Dragging, {begin});
        }
        case GestureEventKind::Drop:
        case GestureEventKind::CancelRequested:
            // Treated as a click/select: no preview was opened, nothing to undo.
            return transition(GestureState::Idle, {});
        case GestureEventKind::OutputGenerationChanged:
            return transition(GestureState::Idle, {});
        default:
            return refuse(event.kind);
        }

    case GestureState::Dragging:
        switch (event.kind) {
        case GestureEventKind::PreviewSettled:
            if (event.ok) {
                return transition(GestureState::Dragging, {});
            }
            // BeginPreview failed: abort with a typed reason and no visual
            // drag, and nothing to cancel because no preview exists.
            m_resolvedTarget.reset();
            return transition(GestureState::Idle, {});
        case GestureEventKind::HoverChanged: {
            QVector<GestureDirective> directives;
            const bool identityChanged =
                !m_resolvedTarget.has_value() || !(*m_resolvedTarget == event.target);
            GestureDirective evaluate;
            evaluate.kind = GestureDirectiveKind::Evaluate;
            evaluate.target = event.target;
            directives.append(evaluate);
            if (identityChanged) {
                // Architecture D3: execute only when the resolved target
                // identity actually changes, never per pointer-motion event.
                GestureDirective execute;
                execute.kind = GestureDirectiveKind::ExecutePending;
                execute.target = event.target;
                directives.append(execute);
                m_resolvedTarget = event.target;
            }
            return transition(GestureState::Dragging, std::move(directives));
        }
        case GestureEventKind::Drop: {
            GestureDirective commit;
            commit.kind = GestureDirectiveKind::CommitPreview;
            return transition(GestureState::Committing, {commit});
        }
        case GestureEventKind::CancelRequested:
            return enterCancelling();
        case GestureEventKind::OutputGenerationChanged:
            // AGENT-GUARD (invariant 6): an output-generation change in any
            // non-idle gesture forces the bracket closed; the session then
            // rebuilds. This must not be skippable, or a gesture would survive
            // an inventory it was solved against.
            return enterCancelling();
        default:
            return refuse(event.kind);
        }

    case GestureState::Committing:
        switch (event.kind) {
        case GestureEventKind::CommitSettled:
            if (event.ok) {
                m_resolvedTarget.reset();
                return transition(GestureState::Idle, {});
            }
            // A failed commit must not leave the bracket half-open; roll the
            // whole gesture back through the reserved cancel revision.
            return enterCancelling();
        case GestureEventKind::OutputGenerationChanged:
            return enterCancelling();
        default:
            return refuse(event.kind);
        }

    case GestureState::Cancelling:
        switch (event.kind) {
        case GestureEventKind::CancelSettled:
            // AGENT-NOTE (invariant 2): CancelPreview cannot fail from
            // revision exhaustion because the engine reserves the final
            // revision, so this transition is unconditional. A failure here
            // still reports a typed error through the session; the engine
            // contract guarantees the pre-gesture profile either way.
            m_resolvedTarget.reset();
            return transition(GestureState::Idle, {});
        case GestureEventKind::OutputGenerationChanged:
            m_resolvedTarget.reset();
            return transition(GestureState::Idle, {});
        default:
            return refuse(event.kind);
        }
    }

    return refuse(event.kind);
}

} // namespace QindaQt::ShellCustomizationEditor
