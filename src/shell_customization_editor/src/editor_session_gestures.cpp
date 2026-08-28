// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/editor_session.h"

#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/editing_engine.h"

#include <QVariant>
#include <QVector>

#include <algorithm>
#include <utility>

// The editor domain consumes the transaction engine public vocabulary
// throughout; the sibling namespace is imported file-locally per convention.
using namespace QindaQt::ShellCustomization;

namespace QindaQt::ShellCustomizationEditor {

namespace {

constexpr auto zoneKey = "zone";
constexpr auto defaultZone = "start";

QString settingsZoneValue(const QVariantMap &settings)
{
    const QVariant value = settings.value(QStringLiteral(zoneKey));
    return value.isValid() ? value.toString() : QStringLiteral(defaultZone);
}

// AGENT-NOTE: UpdateAppletSettingsCommand replaces the whole settings map, so
// every zone companion re-sends the complete captured settings of the dragged
// instance with only the zone replaced. Sending a partial map would silently
// erase the applet's other settings.
QVariantMap settingsWithZone(const QVariantMap &source, const QString &zone)
{
    QVariantMap adjusted = source;
    adjusted.insert(QStringLiteral(zoneKey), zone);
    return adjusted;
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
    const QString zone = m_gestureZone.isEmpty() ? QStringLiteral(defaultZone) : m_gestureZone;
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

bool EditorSession::canUndo() const
{
    return m_machine.state() == GestureState::Idle && m_engine.status().canUndo;
}

bool EditorSession::canRedo() const
{
    return m_machine.state() == GestureState::Idle && m_engine.status().canRedo;
}

void EditorSession::evaluateAcceptance(const QVector<EditingCommand> &candidates,
                                       const DropTarget &target)
{
    DropAcceptance acceptance;
    acceptance.target = target;
    acceptance.revision = observedRevision();
    // Every candidate must be acceptable; a zone-crossing drop is two
    // commands and only one acceptable command would be a half-offer.
    acceptance.accepted = !candidates.isEmpty();
    for (const EditingCommand &candidate : candidates) {
        const EditingEvaluation evaluation = m_engine.evaluate(candidate);
        acceptance.revision = evaluation.revision;
        if (!evaluation.accepted()) {
            acceptance.accepted = false;
            acceptance.reason = evaluation.error.message;
            break;
        }
    }
    m_acceptance = std::move(acceptance);
}

void EditorSession::rollBackGesture()
{
    // CancelRequested → execute the demanded CancelPreview → CancelSettled.
    // The machine treats the settle as unconditional (invariant 2: the final
    // revision is reserved), so the bracket always closes here.
    const GestureTransition cancel = m_machine.handle({GestureEventKind::CancelRequested});
    for (const GestureDirective &directive : cancel.directives) {
        if (directive.kind != GestureDirectiveKind::CancelPreview) {
            continue;
        }
        CancelPreviewCommand command;
        command.expectedRevision = m_chainedRevision;
        const EditingResult result = m_engine.execute(command);
        m_chainedRevision = result.revision;
        m_machine.handle({GestureEventKind::CancelSettled, result.succeeded()});
    }
    m_visualDrag = false;
    m_gestureInstanceId.clear();
    m_gesturePanelId.clear();
    m_gestureZone.clear();
    discardAcceptance();
}

EditorOutcome EditorSession::executePreviewCommands(const QVector<EditingCommand> &commands,
                                                    const DropTarget &appliedTarget)
{
    for (const EditingCommand &command : commands) {
        const EditingResult result = m_engine.execute(command);
        m_chainedRevision = result.revision;
        if (!result.succeeded()) {
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
                m_machine.handle({GestureEventKind::PreviewSettled, result.succeeded()});
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
                m_machine.handle({GestureEventKind::CommitSettled, result.succeeded()});
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
                    m_machine.handle({GestureEventKind::CancelSettled, cancelResult.succeeded()});
                }
                return EditorOutcome::failure(EditorErrorCode::CommandFailed,
                                              result.error.message);
            }
            m_dirty = true;
            break;
        }
        case GestureDirectiveKind::CancelPreview: {
            CancelPreviewCommand command;
            command.expectedRevision = m_chainedRevision;
            const EditingResult result = m_engine.execute(command);
            m_chainedRevision = result.revision;
            const GestureTransition after =
                m_machine.handle({GestureEventKind::CancelSettled, result.succeeded()});
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
    discardAcceptance();
}

EditorOutcome EditorSession::armDrag(const DragPayload &payload)
{
    if (m_stale) {
        return EditorOutcome::failure(
            EditorErrorCode::SessionStale,
            QStringLiteral("the output inventory changed; rebuild the editor session"));
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
    m_gestureZone = QStringLiteral(defaultZone);
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

    const GestureTransition transition = m_machine.handle({GestureEventKind::Arm});
    if (transition.refused) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused, transition.reason);
    }
    settle(EditorOutcome::success());
    return m_lastOutcome;
}

EditorOutcome EditorSession::beginVisualDrag()
{
    if (m_stale) {
        return EditorOutcome::failure(
            EditorErrorCode::SessionStale,
            QStringLiteral("the output inventory changed; rebuild the editor session"));
    }
    const GestureTransition transition = m_machine.handle({GestureEventKind::ThresholdExceeded});
    if (transition.refused) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused, transition.reason);
    }
    const EditorOutcome outcome = runTransition(transition, nullptr, {});
    settle(outcome);
    return outcome;
}

EditorOutcome EditorSession::hoverTarget(const DropTarget &target)
{
    if (m_stale) {
        return EditorOutcome::failure(
            EditorErrorCode::SessionStale,
            QStringLiteral("the output inventory changed; rebuild the editor session"));
    }

    GesturePlan plan;
    plan.intent = gestureIntentFor(target);
    plan.target = target;

    const IntentValidation validation = validateIntent(plan.intent, target);
    if (!validation.ok) {
        // Structurally invalid targets are painted as rejected; they never
        // reach the engine and never open or move a preview.
        DropAcceptance rejection;
        rejection.accepted = false;
        rejection.reason = validation.message;
        rejection.target = target;
        rejection.revision = observedRevision();
        m_acceptance = std::move(rejection);
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
        m_machine.handle({GestureEventKind::HoverChanged, false, target});
    if (transition.refused) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused, transition.reason);
    }

    const EditorOutcome outcome = runTransition(transition, &plan, candidates);
    settle(outcome);
    return outcome;
}

EditorOutcome EditorSession::drop()
{
    const GestureTransition transition = m_machine.handle({GestureEventKind::Drop});
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
    const GestureTransition transition = m_machine.handle({GestureEventKind::CancelRequested});
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
