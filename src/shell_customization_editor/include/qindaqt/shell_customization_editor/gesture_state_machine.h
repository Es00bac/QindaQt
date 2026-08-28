// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::ShellCustomizationEditor {

// One state machine drives pointer and keyboard gestures (architecture §8).
// The machine is the sole authority over gesture state; it only decides WHEN
// engine calls happen, never WHAT they contain — translation stays in the
// pure intent translator and execution in the session.
enum class GestureState {
    Idle,
    Arming,
    Dragging,
    Committing,
    Cancelling,
};

enum class GestureEventKind {
    Arm,                // pointer press on a source, or keyboard Enter/Space
    ThresholdExceeded,  // pointer crossed the drag threshold
    HoverChanged,       // pointer entered a new drop target candidate
    Drop,               // release on a target, or keyboard Space in move mode
    CancelRequested,    // Escape, release off-target, or a failed in-drag command
    PreviewSettled,     // engine replied to BeginPreview (ok = opened)
    CommitSettled,      // engine replied to CommitPreview
    CancelSettled,      // engine replied to CancelPreview
    OutputGenerationChanged,
};

struct GestureEvent final {
    GestureEventKind kind = GestureEventKind::Arm;
    bool ok = false;
    DropTarget target;
};

enum class GestureDirectiveKind {
    BeginPreview,   // open the gesture bracket
    Evaluate,       // evaluate() the hovered target for accept/reject painting
    ExecutePending, // execute the translated commands for a changed target
    CommitPreview,
    CancelPreview,
};

struct GestureDirective final {
    GestureDirectiveKind kind = GestureDirectiveKind::BeginPreview;
    DropTarget target;
};

struct GestureTransition final {
    GestureState state = GestureState::Idle;
    QVector<GestureDirective> directives;
    // Set when the machine refuses an event that makes no sense in the
    // current state; the session surfaces this as a typed outcome.
    bool refused = false;
    QString reason;
};

class GestureStateMachine final {
public:
    // Directives are emitted in engine-call order. The engine is synchronous,
    // so the session executes the directives of one event before feeding the
    // outcome back as the next settled event.
    [[nodiscard]] GestureTransition handle(const GestureEvent &event);

    [[nodiscard]] GestureState state() const noexcept { return m_state; }
    // The target whose commands were last evaluated for execution; reset when
    // a gesture starts or ends. Empty while no resolved target exists.
    [[nodiscard]] std::optional<DropTarget> resolvedTarget() const { return m_resolvedTarget; }

    void reset();

private:
    GestureTransition refuse(GestureEventKind kind);
    GestureTransition transition(GestureState state, QVector<GestureDirective> directives);
    GestureTransition enterCancelling();

    GestureState m_state = GestureState::Idle;
    std::optional<DropTarget> m_resolvedTarget;
};

[[nodiscard]] QString gestureStateName(GestureState state);

} // namespace QindaQt::ShellCustomizationEditor
