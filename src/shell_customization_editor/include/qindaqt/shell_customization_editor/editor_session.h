// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"
#include "qindaqt/shell_customization_editor/gesture_state_machine.h"
#include "qindaqt/shell_customization_editor/intent_translator.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <QString>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace QindaQt::ShellCustomizationEditor {

class EditingEngine;

enum class EditorErrorCode {
    None,
    EngineUnavailable,
    IntentInvalid,
    GestureRefused,
    CommandFailed,
    ApplyFailed,
    SessionStale,
    RebuildRequired,
};

struct EditorOutcome final {
    bool ok = false;
    EditorErrorCode code = EditorErrorCode::None;
    QString message;

    [[nodiscard]] static EditorOutcome success();
    [[nodiscard]] static EditorOutcome failure(EditorErrorCode code, QString message);
};

// Last accept/reject knowledge for one hovered target. Highlights are
// provisional: valid only while the repository stays at `revision` and must
// be discarded after any revision change (architecture invariant 4).
struct DropAcceptance final {
    bool accepted = false;
    QString reason;
    DropTarget target;
    quint64 revision = 0;
};

// Editor session: binds the gesture state machine, the pure intent
// translator, the engine seam, and the user-profile store into the C0
// Customize domain boundary. It owns no repository and no QML: the host
// (settings window) owns the repository and rebuilds it on revert.
//
// Threading: construct and call on the repository's owner thread only. All
// engine calls are synchronous, so one session method completes a whole
// engine turn before returning.
class EditorSession final {
public:
    EditorSession(EditingEngine &engine, UserProfileStore store);

    // -- pointer/keyboard gestures (identical machine, architecture D7) --
    // Capture the payload (pointer press, or keyboard arm on an outline item).
    [[nodiscard]] EditorOutcome armDrag(const DragPayload &payload);
    // Open the preview bracket: pointer crossing the drag threshold, or the
    // keyboard Space arm (which arms and opens in one action).
    [[nodiscard]] EditorOutcome beginVisualDrag();
    [[nodiscard]] EditorOutcome hoverTarget(const DropTarget &target);
    [[nodiscard]] EditorOutcome drop();
    [[nodiscard]] EditorOutcome cancelGesture();

    // -- bracketed point operations (remove/duplicate/configure/insert) --
    // Runs BeginPreview → translated commands → CommitPreview synchronously
    // and rolls the whole bracket back on any failure. The machine stays Idle:
    // undo/redo gating only concerns multi-event drags.
    [[nodiscard]] EditorOutcome applyGesture(const CustomizationIntent &intent,
                                             const DropTarget &target,
                                             const QString &newInstanceAppletId = {});

    // -- session-level operations --
    [[nodiscard]] EditorOutcome undo();
    [[nodiscard]] EditorOutcome redo();
    // Writes the edited profile to the user directory. A failed write keeps
    // the session dirty and changes nothing (architecture D12).
    [[nodiscard]] EditorOutcome applyToUserProfile();
    // Requests discard and returns RebuildRequired without publishing a false
    // clean state. The host must replace this session from the last applied
    // profile; this instance rejects all further edits and persistence.
    [[nodiscard]] EditorOutcome revert();
    // An output-generation change invalidates any open gesture and the whole
    // session (architecture D16); the host must rebuild after this.
    [[nodiscard]] EditorOutcome notifyOutputGenerationChanged();

    // -- presentation queries --
    [[nodiscard]] GestureState gestureState() const noexcept { return m_machine.state(); }
    // True only in Dragging with an actually-open preview: the visual drag
    // ghost must never appear before this (architecture invariant 1).
    [[nodiscard]] bool isVisualDragActive() const noexcept
    {
        return m_machine.state() == GestureState::Dragging && m_visualDrag;
    }
    // Enabled only while no gesture is open (architecture invariant 3).
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] bool isDirty() const noexcept { return m_dirty; }
    // Set once an output-generation change or accepted Revert requires the
    // host to replace this session before any further edit or Apply.
    [[nodiscard]] bool requiresRebuild() const noexcept
    {
        return m_stale || m_rebuildRequired;
    }
    [[nodiscard]] bool isStale() const noexcept { return m_stale; }
    [[nodiscard]] const std::optional<DropAcceptance> &acceptance() const noexcept
    {
        return m_acceptance;
    }
    [[nodiscard]] const EditorOutcome &lastOutcome() const noexcept { return m_lastOutcome; }
    [[nodiscard]] const QString &appliedProfileId() const noexcept { return m_appliedProfileId; }

private:
    struct GesturePlan {
        CustomizationIntent intent;
        DropTarget target;
        TranslationContext context;
    };

    [[nodiscard]] CustomizationIntent gestureIntentFor(const DropTarget &target) const;
    [[nodiscard]] QVariantMap draggedSettingsFor(const DropTarget &target) const;
    [[nodiscard]] EditorOutcome runTransition(const GestureTransition &transition,
                                              const GesturePlan *plan,
                                              const QVector<QindaQt::ShellCustomization::EditingCommand> &candidates);
    [[nodiscard]] EditorOutcome executePreviewCommands(
        const QVector<QindaQt::ShellCustomization::EditingCommand> &commands,
        const DropTarget &appliedTarget);
    void evaluateAcceptance(const QVector<QindaQt::ShellCustomization::EditingCommand> &candidates,
                            const DropTarget &target);
    void rollBackGesture();
    void finishGestureState();
    void settle(EditorOutcome outcome);
    void discardAcceptance();
    [[nodiscard]] quint64 observedRevision() const;
    [[nodiscard]] QString nextInstanceId();

    EditingEngine &m_engine;
    UserProfileStore m_store;
    GestureStateMachine m_machine;
    DragPayload m_payload;
    QString m_gestureInstanceId;
    QString m_gesturePanelId;
    QString m_gestureZone;
    std::optional<DropTarget> m_appliedTarget;
    QVariantMap m_sourceSettings;
    quint64 m_chainedRevision = 0;
    int m_insertCounter = 0;
    bool m_visualDrag = false;
    bool m_currentTargetAccepted = false;
    bool m_dirty = false;
    bool m_stale = false;
    bool m_rebuildRequired = false;
    QString m_appliedProfileId;
    std::optional<DropAcceptance> m_acceptance;
    EditorOutcome m_lastOutcome;
};

} // namespace QindaQt::ShellCustomizationEditor
