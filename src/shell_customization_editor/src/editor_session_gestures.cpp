// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/editor_session.h"

#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/editing_engine.h"

#include "editing_command_sequence_p.h"

#include <QVariant>
#include <QVector>

#include <algorithm>
#include <utility>

// The editor domain consumes the transaction engine public vocabulary
// throughout; the sibling namespace is imported file-locally per convention.
using namespace QindaQt::ShellCustomization;

namespace QindaQt::ShellCustomizationEditor {

namespace {

QString settingsZoneValue(const QVariantMap &settings)
{
    const QVariant value = settings.value(QStringLiteral("zone"));
    return value.isValid() ? value.toString() : QStringLiteral("start");
}

// AGENT-NOTE: UpdateAppletSettingsCommand replaces the whole settings map, so
// every zone companion re-sends the complete captured settings of the dragged
// instance with only the zone replaced. Sending a partial map would silently
// erase the applet's other settings.
QVariantMap settingsWithZone(const QVariantMap &source, const QString &zone)
{
    QVariantMap adjusted = source;
    adjusted.insert(QStringLiteral("zone"), zone);
    return adjusted;
}

GestureEvent gestureEvent(GestureEventKind kind,
                          bool ok = false,
                          DropTarget target = {})
{
    return {kind, ok, std::move(target)};
}

} // namespace

CustomizationIntent EditorSession::gestureIntentFor(const DropTarget &target) const
{
    Q_UNUSED(target);
    if (!m_payload.isPalette()) {
        // AGENT-NOTE (converging execution): inside a preview every accepted
        // target change actually moves the instance, so the next command's
        // source panel is where the instance currently lives in the preview,
        // never the gesture-start panel. Building the second move from the
        // original panel would fail with UnknownAppletId.
        return MoveAppletIntent{m_gesturePanelId, m_payload.sourceAppletId};
    }
    if (m_gestureInstanceId.isEmpty()) {
        return InsertAppletIntent{m_payload.pluginId};
    }
    return MoveAppletIntent{m_gesturePanelId, m_gestureInstanceId};
}

QVariantMap EditorSession::draggedSettingsFor(const DropTarget &target) const
{
    Q_UNUSED(target);
    const QString zone = m_gestureZone.isEmpty() ? QStringLiteral("start") : m_gestureZone;
    return settingsWithZone(m_sourceSettings, zone);
}

void EditorSession::settle(EditorOutcome outcome)
{
    m_lastOutcome = std::move(outcome);
}

void EditorSession::discardAcceptance()
{
    m_acceptance.reset();
}

void EditorSession::refreshDirtyState()
{
    const auto current = m_engine.snapshot();
    // AGENT-GUARD: revisions and history position are not dirty truth. A
    // committed Undo or Redo can return to the exact applied profile at a new
    // revision. Compare the complete canonical schema value, and fail dirty if
    // either side is unavailable or provisional.
    m_dirty = !m_appliedProfileBaseline.has_value() || current == nullptr
        || current->previewActive
        || current->profile.toJson() != *m_appliedProfileBaseline;
}

bool EditorSession::canUndo() const
{
    return !requiresRebuild() && m_machine.state() == GestureState::Idle
        && m_engine.status().canUndo;
}

bool EditorSession::canRedo() const
{
    return !requiresRebuild() && m_machine.state() == GestureState::Idle
        && m_engine.status().canRedo;
}

void EditorSession::evaluateAcceptance(const QVector<EditingCommand> &candidates,
                                       const DropTarget &target)
{
    DropAcceptance acceptance;
    acceptance.target = target;
    acceptance.revision = observedRevision();
    // Every candidate must be acceptable; a zone-crossing drop is two
    // commands and only one acceptable command would be a half-offer.
    const SequenceEvaluation evaluation = m_engine.evaluateSequence(candidates);
    acceptance.accepted = evaluation.accepted;
    acceptance.reason = evaluation.error.message;
    acceptance.revision = evaluation.revision;
    m_currentTargetAccepted = acceptance.accepted;
    m_acceptance = std::move(acceptance);
}

void EditorSession::rollBackGesture()
{
    // CancelRequested → execute the demanded CancelPreview → CancelSettled.
    // The machine treats the settle as unconditional (invariant 2: the final
    // revision is reserved), so the bracket always closes here.
    const GestureTransition cancel =
        m_machine.handle(gestureEvent(GestureEventKind::CancelRequested));
    for (const GestureDirective &directive : cancel.directives) {
        if (directive.kind != GestureDirectiveKind::CancelPreview) {
            continue;
        }
        CancelPreviewCommand command;
        command.expectedRevision = m_chainedRevision;
        const EditingResult result = m_engine.execute(command);
        m_chainedRevision = result.revision;
        const GestureTransition settled = m_machine.handle(
            gestureEvent(GestureEventKind::CancelSettled, result.succeeded()));
        Q_UNUSED(settled);
    }
    m_visualDrag = false;
    m_gestureInstanceId.clear();
    m_gesturePanelId.clear();
    m_gestureZone.clear();
    m_appliedTarget.reset();
    m_currentTargetAccepted = false;
    discardAcceptance();
}

EditorOutcome EditorSession::executePreviewCommands(const QVector<EditingCommand> &commands,
                                                    const DropTarget &appliedTarget)
{
    for (const EditingCommand &command : commands) {
        const EditingResult result =
            m_engine.execute(Internal::retagCommand(command, m_chainedRevision));
        m_chainedRevision = result.revision;
        if (!result.succeeded()) {
            m_currentTargetAccepted = false;
            rollBackGesture();
            return EditorOutcome::failure(EditorErrorCode::CommandFailed, result.error.message);
        }
    }
    m_gesturePanelId = appliedTarget.panelId;
    m_gestureZone = appliedTarget.zone;
    return EditorOutcome::success();
}

EditorOutcome EditorSession::runTransition(const GestureTransition &transition,
                                           const GesturePlan *plan,
                                           const QVector<EditingCommand> &candidates)
{
    for (const GestureDirective &directive : transition.directives) {
        switch (directive.kind) {
        case GestureDirectiveKind::BeginPreview: {
            BeginPreviewCommand command;
            command.expectedRevision = m_chainedRevision;
            const EditingResult result = m_engine.execute(command);
            m_chainedRevision = result.revision;
            const GestureTransition after =
                m_machine.handle(
                    gestureEvent(GestureEventKind::PreviewSettled, result.succeeded()));
            if (!result.succeeded()) {
                // Abort with a typed reason and no visual drag; there is no
                // preview to cancel because none opened.
                m_visualDrag = false;
                return EditorOutcome::failure(EditorErrorCode::CommandFailed,
                                              result.error.message);
            }
            m_visualDrag = m_machine.state() == GestureState::Dragging;
            if (after.refused) {
                return EditorOutcome::failure(EditorErrorCode::GestureRefused, after.reason);
            }
            break;
        }
        case GestureDirectiveKind::Evaluate:
            evaluateAcceptance(candidates, directive.target);
            break;
        case GestureDirectiveKind::ExecutePending:
            if (m_acceptance.has_value() && m_acceptance->accepted
                && m_acceptance->target == directive.target) {
                const EditorOutcome executed =
                    executePreviewCommands(candidates, directive.target);
                if (!executed.ok) {
                    return executed;
                }
                m_currentTargetAccepted = true;
                m_appliedTarget = directive.target;
                if (plan != nullptr && m_payload.isPalette() && m_gestureInstanceId.isEmpty()) {
                    // The executed insert created this instance; the drag now
                    // continues as moves of that instance.
                    m_gestureInstanceId = plan->context.newInstanceAppletId;
                }
                // Invariant 4: the revision moved, so the acceptance computed
                // moments ago is stale and must not linger as a highlight.
                discardAcceptance();
            }
            break;
        case GestureDirectiveKind::CommitPreview: {
            CommitPreviewCommand command;
            command.expectedRevision = m_chainedRevision;
            const EditingResult result = m_engine.execute(command);
            m_chainedRevision = result.revision;
            const GestureTransition after =
                m_machine.handle(
                    gestureEvent(GestureEventKind::CommitSettled, result.succeeded()));
            if (!result.succeeded()) {
                // A failed commit must not leave the bracket open: run the
                // cancel the machine demanded before returning.
                for (const GestureDirective &rollback : after.directives) {
                    if (rollback.kind != GestureDirectiveKind::CancelPreview) {
                        continue;
                    }
                    CancelPreviewCommand cancelCommand;
                    cancelCommand.expectedRevision = m_chainedRevision;
                    const EditingResult cancelResult = m_engine.execute(cancelCommand);
                    m_chainedRevision = cancelResult.revision;
                    const GestureTransition settled = m_machine.handle(
                        gestureEvent(GestureEventKind::CancelSettled,
                                     cancelResult.succeeded()));
                    Q_UNUSED(settled);
                }
                return EditorOutcome::failure(EditorErrorCode::CommandFailed,
                                              result.error.message);
            }
            refreshDirtyState();
            break;
        }
        case GestureDirectiveKind::CancelPreview: {
            CancelPreviewCommand command;
            command.expectedRevision = m_chainedRevision;
            const EditingResult result = m_engine.execute(command);
            m_chainedRevision = result.revision;
            const GestureTransition after =
                m_machine.handle(
                    gestureEvent(GestureEventKind::CancelSettled, result.succeeded()));
            if (!result.succeeded()) {
                return EditorOutcome::failure(EditorErrorCode::CommandFailed,
                                              result.error.message);
            }
            if (after.refused) {
                return EditorOutcome::failure(EditorErrorCode::GestureRefused, after.reason);
            }
            break;
        }
        }
    }
    return EditorOutcome::success();
}

void EditorSession::finishGestureState()
{
    m_visualDrag = false;
    m_gestureInstanceId.clear();
    m_gesturePanelId.clear();
    m_gestureZone.clear();
    m_appliedTarget.reset();
    m_currentTargetAccepted = false;
    discardAcceptance();
}

EditorOutcome EditorSession::armDrag(const DragPayload &payload)
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("rebuild the editor session before editing"));
    }
    if (m_machine.state() != GestureState::Idle) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused,
                                      QStringLiteral("a customization gesture is already active"));
    }

    m_payload = payload;
    finishGestureState();
    m_gesturePanelId = payload.sourcePanelId;

    // Capture the dragged instance's settings and true location from the
    // snapshot the caller observed; the gesture's commands chain from here.
    m_sourceSettings.clear();
    m_gestureZone = QStringLiteral("start");
    if (const auto current = m_engine.snapshot(); current != nullptr && !payload.isPalette()) {
        for (const Profiles::PanelSpec &panel : current->profile.panels) {
            const auto found = std::find_if(panel.applets.cbegin(),
                                            panel.applets.cend(),
                                            [this](const Profiles::AppletSpec &candidate) {
                                                return candidate.id == m_payload.sourceAppletId;
                                            });
            if (found != panel.applets.cend()) {
                m_sourceSettings = found->settings;
                m_gesturePanelId = panel.id;
                m_gestureZone = settingsZoneValue(found->settings);
                break;
            }
        }
    }
    m_chainedRevision = observedRevision();

    const GestureTransition transition =
        m_machine.handle(gestureEvent(GestureEventKind::Arm));
    if (transition.refused) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused, transition.reason);
    }
    settle(EditorOutcome::success());
    return m_lastOutcome;
}

EditorOutcome EditorSession::beginVisualDrag()
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("rebuild the editor session before editing"));
    }
    const GestureTransition transition =
        m_machine.handle(gestureEvent(GestureEventKind::ThresholdExceeded));
    if (transition.refused) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused, transition.reason);
    }
    const EditorOutcome outcome = runTransition(transition, nullptr, {});
    settle(outcome);
    return outcome;
}

EditorOutcome EditorSession::hoverTarget(const DropTarget &target)
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("rebuild the editor session before editing"));
    }

    GesturePlan plan;
    plan.intent = gestureIntentFor(target);
    plan.target = target;

    const IntentValidation validation = validateIntent(plan.intent, target);
    if (!validation.ok()) {
        // Structurally invalid targets are painted as rejected; they never
        // reach the engine and never open or move a preview.
        DropAcceptance rejection;
        rejection.accepted = false;
        rejection.reason = validation.message;
        rejection.target = target;
        rejection.revision = observedRevision();
        m_currentTargetAccepted = false;
        m_acceptance = std::move(rejection);
        // Keep the machine aligned with the physical hover without executing
        // malformed commands. Returning to the prior valid target must then
        // count as an identity change and trigger a fresh evaluation.
        const GestureTransition transition = m_machine.handle(
            gestureEvent(GestureEventKind::HoverChanged, false, target));
        if (transition.refused) {
            return EditorOutcome::failure(EditorErrorCode::GestureRefused,
                                          transition.reason);
        }
        settle(EditorOutcome::success());
        return m_lastOutcome;
    }

    if (intentKind(plan.intent) == IntentKind::InsertApplet) {
        plan.context.newInstanceAppletId =
            m_gestureInstanceId.isEmpty() ? nextInstanceId() : m_gestureInstanceId;
    }
    plan.context.expectedRevision = m_chainedRevision;
    plan.context.sourceSettings = draggedSettingsFor(target);

    const QVector<EditingCommand> candidates = translateIntent(plan.intent, target, plan.context);
    const GestureTransition transition =
        m_machine.handle(gestureEvent(GestureEventKind::HoverChanged, false, target));
    if (transition.refused) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused, transition.reason);
    }

    if (m_appliedTarget.has_value() && *m_appliedTarget == target) {
        // Returning from a rejected/off-target hover to the already-applied
        // provisional target is accepted without replaying a no-op move (the
        // engine correctly rejects such a replay as NoChange).
        m_currentTargetAccepted = true;
        m_acceptance = DropAcceptance{true, {}, target, observedRevision()};
        settle(EditorOutcome::success());
        return m_lastOutcome;
    }

    const EditorOutcome outcome = runTransition(transition, &plan, candidates);
    settle(outcome);
    return outcome;
}

EditorOutcome EditorSession::drop()
{
    if (m_machine.state() == GestureState::Dragging && !m_currentTargetAccepted) {
        // Releasing over an off-target or rejected candidate cancels the whole
        // preview; it must never commit the last provisionally accepted hover.
        return cancelGesture();
    }
    const GestureTransition transition =
        m_machine.handle(gestureEvent(GestureEventKind::Drop));
    if (transition.refused) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused, transition.reason);
    }
    const EditorOutcome outcome = runTransition(transition, nullptr, {});
    finishGestureState();
    settle(outcome);
    return outcome;
}

EditorOutcome EditorSession::cancelGesture()
{
    const GestureTransition transition =
        m_machine.handle(gestureEvent(GestureEventKind::CancelRequested));
    if (transition.refused) {
        // Idle: there was nothing to cancel; that is not a failure.
        settle(EditorOutcome::success());
        return m_lastOutcome;
    }
    const EditorOutcome outcome = runTransition(transition, nullptr, {});
    finishGestureState();
    settle(outcome);
    return outcome;
}

} // namespace QindaQt::ShellCustomizationEditor
