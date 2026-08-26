// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include "layout_candidate_validator_p.h"
#include "layout_edit_preparation_p.h"
#include "layout_edit_request_p.h"
#include "layout_editing_repository_p.h"

#include <utility>

namespace QindaQt::ShellCustomization {
namespace {

EditingError error(EditingErrorCode code, QString message)
{
    return {code, std::move(message), {}, {}};
}

EditingEvaluation rejected(EditingCommandKind kind,
                           EditingError error,
                           quint64 revision)
{
    return {kind, std::move(error), revision};
}

EditingEvaluation accepted(EditingCommandKind kind, quint64 revision)
{
    return {kind, {}, revision};
}

} // namespace

EditingEvaluation LayoutEditingCoordinator::evaluate(
    const EditingCommand &command) const
{
    const auto &session = *m_repository.m_session;
    const EditingCommandKind kind = commandKind(command);
    const quint64 revision = session.snapshot
        ? session.snapshot->revision
        : session.initialRevision;
    if (session.executing) {
        return rejected(
            kind,
            error(EditingErrorCode::ReentrantExecution,
                  QStringLiteral("layout editing execution is not reentrant")),
            revision);
    }
    if (auto preflightError = editingRequestError(m_repository.isReady(),
                                                  m_repository.initializationError(),
                                                  expectedRevision(command),
                                                  revision,
                                                  session.preview.has_value(),
                                                  kind)) {
        return rejected(kind, std::move(*preflightError), revision);
    }

    switch (kind) {
    case EditingCommandKind::Undo:
    case EditingCommandKind::Redo: {
        const bool undo = kind == EditingCommandKind::Undo;
        const QVector<Profiles::LayoutProfile> &history = session.preview.has_value()
            ? (undo ? session.preview->undo : session.preview->redo)
            : (undo ? session.undo : session.redo);
        if (history.isEmpty()) {
            return rejected(
                kind,
                error(undo ? EditingErrorCode::NothingToUndo
                           : EditingErrorCode::NothingToRedo,
                      undo ? QStringLiteral("there is no layout edit to undo")
                           : QStringLiteral("there is no layout edit to redo")),
                revision);
        }
        CandidateValidation validation =
            LayoutCandidateValidator::validate(history.constLast(),
                                               m_repository.outputs());
        return validation.succeeded()
            ? accepted(kind, revision)
            : rejected(kind, std::move(validation.error), revision);
    }
    case EditingCommandKind::BeginPreview:
        return session.preview.has_value()
            ? rejected(
                  kind,
                  error(EditingErrorCode::PreviewAlreadyActive,
                        QStringLiteral("a layout preview is already active")),
                  revision)
            : accepted(kind, revision);
    case EditingCommandKind::CommitPreview:
        return session.preview.has_value()
            ? accepted(kind, revision)
            : rejected(
                  kind,
                  error(EditingErrorCode::PreviewNotActive,
                        QStringLiteral("there is no active layout preview to commit")),
                  revision);
    case EditingCommandKind::CancelPreview: {
        if (!session.preview.has_value()) {
            return rejected(
                kind,
                error(EditingErrorCode::PreviewNotActive,
                      QStringLiteral("there is no active layout preview to cancel")),
                revision);
        }
        CandidateValidation validation =
            LayoutCandidateValidator::validate(*session.preview->baseProfile,
                                               m_repository.outputs());
        return validation.succeeded()
            ? accepted(kind, revision)
            : rejected(kind, std::move(validation.error), revision);
    }
    case EditingCommandKind::AddPanel:
    case EditingCommandKind::RemovePanel:
    case EditingCommandKind::MovePanel:
    case EditingCommandKind::ConfigurePanel:
    case EditingCommandKind::InsertApplet:
    case EditingCommandKind::MoveApplet:
    case EditingCommandKind::RemoveApplet:
    case EditingCommandKind::DuplicateApplet:
    case EditingCommandKind::UpdateAppletSettings: {
        CandidateValidation validation = LayoutEditPreparation::prepare(
            session.snapshot->profile,
            command,
            session.placementValidator,
            m_repository.outputs());
        return validation.succeeded()
            ? accepted(kind, revision)
            : rejected(kind, std::move(validation.error), revision);
    }
    }
    return rejected(
        kind,
        error(EditingErrorCode::InvalidCommand,
              QStringLiteral("unknown layout editing command")),
        revision);
}

} // namespace QindaQt::ShellCustomization
