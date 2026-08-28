// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/coordinator_engine_adapter.h"

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

EditingEvaluation unavailableEvaluation(const EditingCommand &command)
{
    EditingEvaluation evaluation;
    evaluation.kind = commandKind(command);
    evaluation.error = unavailableResult(command).error;
    return evaluation;
}

} // namespace

CoordinatorEditingEngine::CoordinatorEditingEngine(LayoutEditingRepository &repository)
    : m_repository(repository)
    , m_coordinator(m_repository.tryAcquireCoordinator())
{
}

CoordinatorEditingEngine::~CoordinatorEditingEngine() = default;

EditingResult CoordinatorEditingEngine::execute(const EditingCommand &command)
{
    if (m_coordinator == nullptr) {
        return unavailableResult(command);
    }
    return m_coordinator->execute(command);
}

EditingEvaluation CoordinatorEditingEngine::evaluate(const EditingCommand &command) const
{
    if (m_coordinator == nullptr) {
        return unavailableEvaluation(command);
    }
    return m_coordinator->evaluate(command);
}

std::shared_ptr<const LayoutEditingSnapshot> CoordinatorEditingEngine::snapshot() const
{
    return m_repository.snapshot();
}

bool CoordinatorEditingEngine::hasPreview() const
{
    if (m_coordinator == nullptr) {
        return false;
    }
    return m_coordinator->hasPreview();
}

bool CoordinatorEditingEngine::holdsLease() const noexcept
{
    return m_coordinator != nullptr;
}

} // namespace QindaQt::ShellCustomizationEditor
