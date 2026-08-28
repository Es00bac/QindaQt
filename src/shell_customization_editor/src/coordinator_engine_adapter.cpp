// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/coordinator_engine_adapter.h"

#include "editing_command_sequence_p.h"
#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include <QThread>

#include <utility>

// The editor domain consumes the transaction engine public vocabulary
// throughout; the sibling namespace is imported file-locally per convention.
using namespace QindaQt::ShellCustomization;

namespace QindaQt::ShellCustomizationEditor {

namespace {

EditingResult unavailableResult(const EditingCommand &command)
{
    EditingResult result;
    result.kind = commandKind(command);
    result.error.code = EditingErrorCode::RepositoryNotReady;
    result.error.message = QStringLiteral(
        "the layout editing session is owned by another window");
    return result;
}

EditingResult wrongThreadResult(const EditingCommand &command)
{
    EditingResult result;
    result.kind = commandKind(command);
    result.error.code = EditingErrorCode::RepositoryNotReady;
    result.error.message = QStringLiteral(
        "the layout editor engine was called from outside its owner thread");
    return result;
}

EditingEvaluation unavailableEvaluation(const EditingCommand &command)
{
    EditingEvaluation evaluation;
    evaluation.kind = commandKind(command);
    evaluation.error = unavailableResult(command).error;
    return evaluation;
}

} // namespace

CoordinatorEditingEngine::CoordinatorEditingEngine(
    LayoutEditingRepository &repository,
    QVector<Applets::AppletManifest> manifestCatalog)
    : m_repository(repository)
    , m_manifestCatalog(std::move(manifestCatalog))
    , m_ownerThread(QThread::currentThread())
    , m_coordinator(m_repository.tryAcquireCoordinator())
{
}

CoordinatorEditingEngine::~CoordinatorEditingEngine() = default;

EditingResult CoordinatorEditingEngine::execute(const EditingCommand &command)
{
    if (!onOwnerThread()) {
        return wrongThreadResult(command);
    }
    if (!ensureCoordinator()) {
        return unavailableResult(command);
    }
    return m_coordinator->execute(command);
}

EditingEvaluation CoordinatorEditingEngine::evaluate(const EditingCommand &command) const
{
    if (!onOwnerThread()) {
        return unavailableEvaluation(command);
    }
    if (!ensureCoordinator()) {
        return unavailableEvaluation(command);
    }
    return m_coordinator->evaluate(command);
}

SequenceEvaluation CoordinatorEditingEngine::evaluateSequence(
    const QVector<EditingCommand> &commands) const
{
    SequenceEvaluation outcome;
    const auto current = snapshot();
    if (!onOwnerThread() || current == nullptr || !ensureCoordinator()) {
        outcome.error.code = EditingErrorCode::RepositoryNotReady;
        outcome.error.message = QStringLiteral(
            "the layout editing session is unavailable for sequence evaluation");
        return outcome;
    }

    // AGENT-GUARD: sequence acceptance must never advance the live repository.
    // A disposable repository runs the exact public mutation/manifest/layout
    // pipeline against the current provisional profile and identical inputs.
    LayoutEditingRepository probe(current->profile,
                                  m_repository.outputs(),
                                  m_manifestCatalog,
                                  current->revision);
    if (!probe.isReady()) {
        outcome.error = probe.initializationError();
        return outcome;
    }
    auto coordinator = probe.tryAcquireCoordinator();
    if (coordinator == nullptr) {
        outcome.error.code = EditingErrorCode::RepositoryNotReady;
        outcome.error.message = QStringLiteral(
            "the sequence evaluation repository could not acquire its coordinator");
        return outcome;
    }
    quint64 revision = current->revision;
    for (const EditingCommand &candidate : commands) {
        const EditingResult result = coordinator->execute(
            Internal::retagCommand(candidate, revision));
        if (!result.succeeded()) {
            outcome.error = result.error;
            outcome.revision = current->revision;
            return outcome;
        }
        revision = result.revision;
    }
    outcome.accepted = !commands.isEmpty();
    outcome.revision = current->revision;
    return outcome;
}

std::shared_ptr<const LayoutEditingSnapshot> CoordinatorEditingEngine::snapshot() const
{
    if (!onOwnerThread()) {
        return nullptr;
    }
    return m_repository.snapshot();
}

bool CoordinatorEditingEngine::isReady() const
{
    return onOwnerThread() && ensureCoordinator();
}

std::shared_ptr<const Profiles::LayoutProfile>
CoordinatorEditingEngine::committedProfile() const
{
    if (!isReady()) {
        return nullptr;
    }
    return m_coordinator->committedProfile();
}

LayoutEditingStatus CoordinatorEditingEngine::status() const
{
    if (!onOwnerThread() || !ensureCoordinator()) {
        return {};
    }
    return m_repository.status();
}

bool CoordinatorEditingEngine::hasPreview() const
{
    if (!onOwnerThread() || !ensureCoordinator()) {
        return false;
    }
    return m_coordinator->hasPreview();
}

bool CoordinatorEditingEngine::holdsLease() const
{
    return isReady();
}

bool CoordinatorEditingEngine::onOwnerThread() const noexcept
{
    return QThread::currentThread() == m_ownerThread;
}

bool CoordinatorEditingEngine::ensureCoordinator() const
{
    if (!onOwnerThread()) {
        return false;
    }
    if (m_coordinator == nullptr) {
        // A losing editor retries on its next action after the former lease
        // owner exits; read-only is not a permanent construction-time state.
        m_coordinator = m_repository.tryAcquireCoordinator();
    }
    return m_coordinator != nullptr;
}

} // namespace QindaQt::ShellCustomizationEditor
