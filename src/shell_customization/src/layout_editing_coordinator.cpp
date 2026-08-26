// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include "layout_candidate_validator_p.h"
#include "layout_edit_helpers_p.h"
#include "layout_edit_preparation_p.h"
#include "layout_edit_request_p.h"
#include "layout_editing_repository_p.h"

#include <utility>

namespace QindaQt::ShellCustomization {
namespace {

EditingResult failure(EditingCommandKind kind,
                      EditingError error,
                      quint64 revision)
{
    return {kind, std::move(error), revision, revision};
}

EditingResult success(EditingCommandKind kind,
                      quint64 previousRevision,
                      quint64 revision)
{
    return {kind, {}, previousRevision, revision};
}

EditingError error(EditingErrorCode code, QString message)
{
    return {code, std::move(message), {}, {}};
}

std::shared_ptr<const Profiles::LayoutProfile> profileAlias(
    const std::shared_ptr<const LayoutEditingSnapshot> &snapshot) noexcept
{
    // AGENT-NOTE: The alias retains the immutable snapshot while avoiding a
    // second profile allocation after history mutation has begun.
    return {snapshot, &snapshot->profile};
}

class ExecutionGuard final {
public:
    explicit ExecutionGuard(bool &executing)
        : m_executing(executing)
    {
        m_executing = true;
    }

    ~ExecutionGuard() { m_executing = false; }

private:
    bool &m_executing;
};

} // namespace

LayoutEditingCoordinator::LayoutEditingCoordinator(LayoutEditingRepository &repository)
    : m_repository(repository)
{
}

LayoutEditingCoordinator::~LayoutEditingCoordinator()
{
    m_repository.releaseCoordinator();
}

EditingResult LayoutEditingCoordinator::execute(const EditingCommand &command)
{
    auto &session = *m_repository.m_session;
    const EditingCommandKind kind = commandKind(command);
    const quint64 beforeRevision = session.snapshot
        ? session.snapshot->revision
        : session.initialRevision;
    if (session.executing) {
        return failure(kind,
                       error(EditingErrorCode::ReentrantExecution,
                             QStringLiteral("layout editing execution is not reentrant")),
                       beforeRevision);
    }
    ExecutionGuard guard(session.executing);

    if (auto preflightError = editingRequestError(m_repository.isReady(),
                                                  m_repository.initializationError(),
                                                  expectedRevision(command),
                                                  beforeRevision,
                                                  session.preview.has_value(),
                                                  kind)) {
        return failure(kind, std::move(*preflightError), beforeRevision);
    }

    switch (kind) {
    case EditingCommandKind::Undo:
        return undo(kind, beforeRevision);
    case EditingCommandKind::Redo:
        return redo(kind, beforeRevision);
    case EditingCommandKind::BeginPreview:
        return beginPreview(kind, beforeRevision);
    case EditingCommandKind::CommitPreview:
        return commitPreview(kind, beforeRevision);
    case EditingCommandKind::CancelPreview:
        return cancelPreview(kind, beforeRevision);
    case EditingCommandKind::AddPanel:
    case EditingCommandKind::RemovePanel:
    case EditingCommandKind::MovePanel:
    case EditingCommandKind::ConfigurePanel:
    case EditingCommandKind::InsertApplet:
    case EditingCommandKind::MoveApplet:
    case EditingCommandKind::RemoveApplet:
    case EditingCommandKind::DuplicateApplet:
    case EditingCommandKind::UpdateAppletSettings:
        return executeEdit(command, kind, beforeRevision);
    }
    return failure(kind,
                   error(EditingErrorCode::InvalidCommand,
                         QStringLiteral("unknown layout editing command")),
                   beforeRevision);
}

std::shared_ptr<const Profiles::LayoutProfile>
LayoutEditingCoordinator::committedProfile() const noexcept
{
    return m_repository.m_session->committedProfile;
}

bool LayoutEditingCoordinator::hasPreview() const noexcept
{
    return m_repository.m_session->preview.has_value();
}

EditingResult LayoutEditingCoordinator::executeEdit(const EditingCommand &command,
                                                    EditingCommandKind kind,
                                                    quint64 beforeRevision)
{
    auto &session = *m_repository.m_session;
    const auto before = session.snapshot;
    CandidateValidation validation = LayoutEditPreparation::prepare(
        before->profile,
        command,
        session.placementValidator,
        m_repository.outputs());
    if (!validation.succeeded()) {
        return failure(kind, std::move(validation.error), beforeRevision);
    }

    const bool nextPreviewDirty = session.preview.has_value()
        && !LayoutEditHelpers::sameProfile(*session.preview->baseProfile,
                                           validation.profile);

    auto next = std::make_shared<const LayoutEditingSnapshot>(
        LayoutEditingSnapshot{std::move(validation.profile),
                              std::move(validation.layout),
                              beforeRevision + 1,
                              session.preview.has_value()});
    std::shared_ptr<const Profiles::LayoutProfile> nextCommitted;
    if (!session.preview.has_value()) {
        nextCommitted = profileAlias(next);
    }

    if (session.preview.has_value()) {
        session.preview->undo.append(before->profile);
        session.preview->redo.clear();
        session.previewDirty = nextPreviewDirty;
    } else {
        session.undo.append(*session.committedProfile);
        session.redo.clear();
        session.committedProfile = std::move(nextCommitted);
        session.previewDirty = false;
    }

    // AGENT-GUARD: Mutation, schema validation, compatibility checks, and the
    // all-output solve and result allocations finish before history or the
    // repository can expose the new revision. Returned failures retain both
    // prior pixels and undo boundaries.
    m_repository.publish(std::move(next));
    return success(kind, beforeRevision, beforeRevision + 1);
}

EditingResult LayoutEditingCoordinator::beginPreview(EditingCommandKind kind,
                                                     quint64 beforeRevision)
{
    auto &session = *m_repository.m_session;
    if (session.preview.has_value()) {
        return failure(kind,
                       error(EditingErrorCode::PreviewAlreadyActive,
                             QStringLiteral("a layout preview is already active")),
                       beforeRevision);
    }

    const auto before = session.snapshot;
    auto next = std::make_shared<const LayoutEditingSnapshot>(
        LayoutEditingSnapshot{before->profile,
                              before->layout,
                              beforeRevision + 1,
                              true});
    session.preview = LayoutEditingRepository::SessionState::PreviewHistory{
        session.committedProfile, {}, {}};
    session.previewDirty = false;
    m_repository.publish(std::move(next));
    return success(kind, beforeRevision, beforeRevision + 1);
}

EditingResult LayoutEditingCoordinator::commitPreview(EditingCommandKind kind,
                                                      quint64 beforeRevision)
{
    auto &session = *m_repository.m_session;
    if (!session.preview.has_value()) {
        return failure(kind,
                       error(EditingErrorCode::PreviewNotActive,
                             QStringLiteral("there is no active layout preview to commit")),
                       beforeRevision);
    }

    const auto before = session.snapshot;
    const bool changed = !LayoutEditHelpers::sameProfile(
        *session.preview->baseProfile, before->profile);
    auto next = std::make_shared<const LayoutEditingSnapshot>(
        LayoutEditingSnapshot{before->profile,
                              before->layout,
                              beforeRevision + 1,
                              false});
    std::shared_ptr<const Profiles::LayoutProfile> nextCommitted;
    if (changed) {
        nextCommitted = profileAlias(next);
    }

    if (changed) {
        session.undo.append(*session.preview->baseProfile);
        session.redo.clear();
        session.committedProfile = std::move(nextCommitted);
    }
    session.preview.reset();
    session.previewDirty = false;
    m_repository.publish(std::move(next));
    return success(kind, beforeRevision, beforeRevision + 1);
}

EditingResult LayoutEditingCoordinator::cancelPreview(EditingCommandKind kind,
                                                      quint64 beforeRevision)
{
    auto &session = *m_repository.m_session;
    if (!session.preview.has_value()) {
        return failure(kind,
                       error(EditingErrorCode::PreviewNotActive,
                             QStringLiteral("there is no active layout preview to cancel")),
                       beforeRevision);
    }

    CandidateValidation validation =
        LayoutCandidateValidator::validate(*session.preview->baseProfile,
                                           m_repository.outputs());
    if (!validation.succeeded()) {
        return failure(kind, std::move(validation.error), beforeRevision);
    }
    auto next = std::make_shared<const LayoutEditingSnapshot>(
        LayoutEditingSnapshot{std::move(validation.profile),
                              std::move(validation.layout),
                              beforeRevision + 1,
                              false});
    session.preview.reset();
    session.previewDirty = false;
    m_repository.publish(std::move(next));
    return success(kind, beforeRevision, beforeRevision + 1);
}

EditingResult LayoutEditingCoordinator::undo(EditingCommandKind kind,
                                             quint64 beforeRevision)
{
    auto &session = *m_repository.m_session;
    QVector<Profiles::LayoutProfile> *undoStack =
        session.preview.has_value() ? &session.preview->undo : &session.undo;
    QVector<Profiles::LayoutProfile> *redoStack =
        session.preview.has_value() ? &session.preview->redo : &session.redo;
    if (undoStack->isEmpty()) {
        return failure(kind,
                       error(EditingErrorCode::NothingToUndo,
                             QStringLiteral("there is no layout edit to undo")),
                       beforeRevision);
    }

    CandidateValidation validation =
        LayoutCandidateValidator::validate(undoStack->constLast(),
                                           m_repository.outputs());
    if (!validation.succeeded()) {
        return failure(kind, std::move(validation.error), beforeRevision);
    }

    const auto before = session.snapshot;
    auto next = std::make_shared<const LayoutEditingSnapshot>(
        LayoutEditingSnapshot{std::move(validation.profile),
                              std::move(validation.layout),
                              beforeRevision + 1,
                              session.preview.has_value()});
    const bool nextPreviewDirty = session.preview.has_value()
        && !LayoutEditHelpers::sameProfile(*session.preview->baseProfile,
                                           next->profile);
    std::shared_ptr<const Profiles::LayoutProfile> nextCommitted;
    if (!session.preview.has_value()) {
        nextCommitted = profileAlias(next);
    }
    redoStack->append(before->profile);
    undoStack->removeLast();
    if (!session.preview.has_value()) {
        session.committedProfile = std::move(nextCommitted);
    }
    session.previewDirty = nextPreviewDirty;
    m_repository.publish(std::move(next));
    return success(kind, beforeRevision, beforeRevision + 1);
}

EditingResult LayoutEditingCoordinator::redo(EditingCommandKind kind,
                                             quint64 beforeRevision)
{
    auto &session = *m_repository.m_session;
    QVector<Profiles::LayoutProfile> *undoStack =
        session.preview.has_value() ? &session.preview->undo : &session.undo;
    QVector<Profiles::LayoutProfile> *redoStack =
        session.preview.has_value() ? &session.preview->redo : &session.redo;
    if (redoStack->isEmpty()) {
        return failure(kind,
                       error(EditingErrorCode::NothingToRedo,
                             QStringLiteral("there is no layout edit to redo")),
                       beforeRevision);
    }

    CandidateValidation validation =
        LayoutCandidateValidator::validate(redoStack->constLast(),
                                           m_repository.outputs());
    if (!validation.succeeded()) {
        return failure(kind, std::move(validation.error), beforeRevision);
    }

    const auto before = session.snapshot;
    auto next = std::make_shared<const LayoutEditingSnapshot>(
        LayoutEditingSnapshot{std::move(validation.profile),
                              std::move(validation.layout),
                              beforeRevision + 1,
                              session.preview.has_value()});
    const bool nextPreviewDirty = session.preview.has_value()
        && !LayoutEditHelpers::sameProfile(*session.preview->baseProfile,
                                           next->profile);
    std::shared_ptr<const Profiles::LayoutProfile> nextCommitted;
    if (!session.preview.has_value()) {
        nextCommitted = profileAlias(next);
    }
    undoStack->append(before->profile);
    redoStack->removeLast();
    if (!session.preview.has_value()) {
        session.committedProfile = std::move(nextCommitted);
    }
    session.previewDirty = nextPreviewDirty;
    m_repository.publish(std::move(next));
    return success(kind, beforeRevision, beforeRevision + 1);
}

} // namespace QindaQt::ShellCustomization
