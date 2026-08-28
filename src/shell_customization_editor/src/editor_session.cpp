// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/editor_session.h"

#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/editing_engine.h"

#include "editing_command_sequence_p.h"

#include <QVariant>
#include <QVector>

#include <algorithm>
#include <utility>
#include <variant>

// The editor domain consumes the transaction engine public vocabulary
// throughout; the sibling namespace is imported file-locally per convention.
using namespace QindaQt::ShellCustomization;

namespace QindaQt::ShellCustomizationEditor {

namespace {

// Returns the (panel, applet) pair a move or duplicate gesture operates on.
bool moveSubject(const CustomizationIntent &intent, QString *panelId, QString *appletId)
{
    if (const auto *move = std::get_if<MoveAppletIntent>(&intent)) {
        *panelId = move->panelId;
        *appletId = move->appletId;
        return true;
    }
    if (const auto *duplicate = std::get_if<DuplicateAppletIntent>(&intent)) {
        *panelId = duplicate->panelId;
        *appletId = duplicate->appletId;
        return true;
    }
    return false;
}

} // namespace

EditorOutcome EditorOutcome::success()
{
    EditorOutcome outcome;
    outcome.ok = true;
    return outcome;
}

EditorOutcome EditorOutcome::failure(EditorErrorCode code, QString message)
{
    EditorOutcome outcome;
    outcome.ok = false;
    outcome.code = code;
    outcome.message = std::move(message);
    return outcome;
}

EditorSession::EditorSession(EditingEngine &engine, UserProfileStore store)
    : m_engine(engine)
    , m_store(std::move(store))
{
}

quint64 EditorSession::observedRevision() const
{
    const auto current = m_engine.snapshot();
    // A null snapshot means the repository never initialized; the engine
    // rejects every command with a typed error either way.
    return current ? current->revision : 0;
}

QString EditorSession::nextInstanceId()
{
    ++m_insertCounter;
    return m_payload.pluginId + QLatin1String("-instance-") + QString::number(m_insertCounter);
}

EditorOutcome EditorSession::applyGesture(const CustomizationIntent &intent,
                                          const DropTarget &target,
                                          const QString &newInstanceAppletId)
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("rebuild the editor session before editing"));
    }
    if (m_machine.state() != GestureState::Idle || m_engine.hasPreview()) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused,
                                      QStringLiteral("a customization preview is already active"));
    }
    const IntentValidation validation = validateIntent(intent, target);
    if (!validation.ok()) {
        return EditorOutcome::failure(EditorErrorCode::IntentInvalid, validation.message);
    }

    TranslationContext context;
    context.expectedRevision = observedRevision();
    context.newInstanceAppletId = newInstanceAppletId;
    QString subjectPanelId;
    QString subjectAppletId;
    if (moveSubject(intent, &subjectPanelId, &subjectAppletId)) {
        // AGENT-NOTE: point operations read the source settings from the same
        // state the commands target, which is always the committed state here
        // because the machine guarantees no preview is open.
        context.sourceSettings.clear();
        if (const auto current = m_engine.snapshot(); current != nullptr) {
            for (const Profiles::PanelSpec &panel : current->profile.panels) {
                const auto found = std::find_if(panel.applets.cbegin(),
                                                panel.applets.cend(),
                                                [&subjectAppletId](const Profiles::AppletSpec &candidate) {
                                                    return candidate.id == subjectAppletId;
                                                });
                if (found != panel.applets.cend()) {
                    context.sourceSettings = found->settings;
                    break;
                }
            }
        }
    }

    const QVector<EditingCommand> mutations = translateIntent(intent, target, context);
    QVector<EditingCommand> bracketed;
    bracketed.reserve(mutations.size() + 2);
    BeginPreviewCommand begin;
    begin.expectedRevision = context.expectedRevision;
    bracketed.append(begin);
    bracketed.append(mutations);
    CommitPreviewCommand commit;
    commit.expectedRevision = context.expectedRevision;
    bracketed.append(commit);

    quint64 chained = context.expectedRevision;
    bool previewOpen = false;
    for (const EditingCommand &command : bracketed) {
        const EditingResult result =
            m_engine.execute(Internal::retagCommand(command, chained));
        chained = result.revision;
        if (!result.succeeded()) {
            if (previewOpen) {
                CancelPreviewCommand cancelCommand;
                cancelCommand.expectedRevision = chained;
                const EditingResult cancelResult = m_engine.execute(cancelCommand);
                chained = cancelResult.revision;
            }
            settle(EditorOutcome::failure(EditorErrorCode::CommandFailed,
                                          result.error.message));
            return m_lastOutcome;
        }
        if (commandKind(command) == EditingCommandKind::BeginPreview) {
            previewOpen = true;
        }
    }
    m_chainedRevision = chained;
    discardAcceptance();
    m_dirty = true;
    settle(EditorOutcome::success());
    return m_lastOutcome;
}

EditorOutcome EditorSession::undo()
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("rebuild the editor session before editing"));
    }
    // AGENT-GUARD (invariant 3): history controls stay disabled while any
    // gesture is open; interleaving durable history commands with an open
    // provisional bracket would corrupt the one-undo-step-per-gesture rule.
    if (m_machine.state() != GestureState::Idle) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused,
                                      QStringLiteral("finish or cancel the active gesture first"));
    }
    UndoCommand command;
    command.expectedRevision = observedRevision();
    const EditingResult result = m_engine.execute(command);
    discardAcceptance();
    if (!result.succeeded()) {
        settle(EditorOutcome::failure(EditorErrorCode::CommandFailed, result.error.message));
        return m_lastOutcome;
    }
    m_dirty = true;
    settle(EditorOutcome::success());
    return m_lastOutcome;
}

EditorOutcome EditorSession::redo()
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("rebuild the editor session before editing"));
    }
    if (m_machine.state() != GestureState::Idle) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused,
                                      QStringLiteral("finish or cancel the active gesture first"));
    }
    RedoCommand command;
    command.expectedRevision = observedRevision();
    const EditingResult result = m_engine.execute(command);
    discardAcceptance();
    if (!result.succeeded()) {
        settle(EditorOutcome::failure(EditorErrorCode::CommandFailed, result.error.message));
        return m_lastOutcome;
    }
    m_dirty = true;
    settle(EditorOutcome::success());
    return m_lastOutcome;
}

EditorOutcome EditorSession::applyToUserProfile()
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("rebuild the editor session before applying"));
    }
    if (m_machine.state() != GestureState::Idle || m_engine.hasPreview()) {
        return EditorOutcome::failure(
            EditorErrorCode::GestureRefused,
            QStringLiteral("finish or cancel the provisional gesture before applying"));
    }
    const auto current = m_engine.snapshot();
    if (current == nullptr || current->previewActive) {
        return EditorOutcome::failure(EditorErrorCode::EngineUnavailable,
                                      QStringLiteral("no committed layout is available for applying"));
    }
    const ProfileStoreResult result = m_store.save(current->profile);
    if (!result.ok()) {
        // Deterministic rollback: the selection and the dirty flag stay
        // unchanged, and the typed reason is surfaced verbatim.
        settle(EditorOutcome::failure(EditorErrorCode::ApplyFailed, result.message));
        return m_lastOutcome;
    }
    m_appliedProfileId = current->profile.id;
    m_dirty = false;
    settle(EditorOutcome::success());
    return m_lastOutcome;
}

EditorOutcome EditorSession::revert()
{
    if (requiresRebuild()) {
        return EditorOutcome::failure(
            m_stale ? EditorErrorCode::SessionStale : EditorErrorCode::RebuildRequired,
            QStringLiteral("the editor session already requires a rebuild"));
    }
    if (m_machine.state() != GestureState::Idle) {
        return EditorOutcome::failure(EditorErrorCode::GestureRefused,
                                      QStringLiteral("finish or cancel the active gesture first"));
    }
    discardAcceptance();
    m_rebuildRequired = true;
    // AGENT-GUARD: no collaborator in this domain can replace the repository.
    // Keep dirty truth observable until the host constructs the replacement;
    // clearing it here would allow this edited snapshot to be applied later.
    settle(EditorOutcome::failure(
        EditorErrorCode::RebuildRequired,
        QStringLiteral("revert requested; rebuild from the last applied profile")));
    return m_lastOutcome;
}

EditorOutcome EditorSession::notifyOutputGenerationChanged()
{
    if (m_machine.state() != GestureState::Idle) {
        const EditorOutcome cancellation = cancelGesture();
        Q_UNUSED(cancellation);
    }
    m_stale = true;
    m_machine.reset();
    finishGestureState();
    settle(EditorOutcome::failure(
        EditorErrorCode::SessionStale,
        QStringLiteral("the output inventory changed; rebuild the editor session")));
    return m_lastOutcome;
}

} // namespace QindaQt::ShellCustomizationEditor
