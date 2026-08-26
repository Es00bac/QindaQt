// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization/layout_editing_repository.h"

#include "layout_candidate_validator_p.h"
#include "layout_editing_repository_p.h"
#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include <utility>

namespace QindaQt::ShellCustomization {

LayoutEditingRepository::LayoutEditingRepository(
    Profiles::LayoutProfile initialProfile,
    QVector<ShellLayout::LogicalOutput> outputs,
    quint64 initialRevision)
    : LayoutEditingRepository(std::move(initialProfile),
                              std::move(outputs),
                              {},
                              initialRevision)
{
}

LayoutEditingRepository::LayoutEditingRepository(
    Profiles::LayoutProfile initialProfile,
    QVector<ShellLayout::LogicalOutput> outputs,
    QVector<Applets::AppletManifest> manifestCatalog,
    quint64 initialRevision)
    : m_outputs(std::move(outputs))
    , m_session(std::make_unique<SessionState>(std::move(manifestCatalog),
                                               initialRevision))
{
    const EditingError &catalogError =
        m_session->placementValidator.initializationError();
    if (catalogError.code != EditingErrorCode::None) {
        m_session->initializationError = catalogError;
        return;
    }

    CandidateValidation validation =
        LayoutCandidateValidator::validate(initialProfile, m_outputs);
    if (!validation.succeeded()) {
        m_session->initializationError = std::move(validation.error);
        return;
    }

    m_session->snapshot = std::make_shared<const LayoutEditingSnapshot>(
        LayoutEditingSnapshot{std::move(validation.profile),
                              std::move(validation.layout),
                              initialRevision,
                              false});
    m_session->committedProfile =
        std::shared_ptr<const Profiles::LayoutProfile>(m_session->snapshot,
                                                       &m_session->snapshot->profile);
}

LayoutEditingRepository::~LayoutEditingRepository() = default;

bool LayoutEditingRepository::isReady() const noexcept
{
    return m_session->initializationError.code == EditingErrorCode::None;
}

const EditingError &LayoutEditingRepository::initializationError() const noexcept
{
    return m_session->initializationError;
}

std::shared_ptr<const LayoutEditingSnapshot> LayoutEditingRepository::snapshot() const noexcept
{
    return m_session->snapshot;
}

LayoutEditingStatus LayoutEditingRepository::status() const noexcept
{
    const bool previewActive = m_session->preview.has_value();
    const QVector<Profiles::LayoutProfile> &undo =
        previewActive ? m_session->preview->undo : m_session->undo;
    const QVector<Profiles::LayoutProfile> &redo =
        previewActive ? m_session->preview->redo : m_session->redo;
    return {
        .previewActive = previewActive,
        .previewDirty = previewActive && m_session->previewDirty,
        .canUndo = !undo.isEmpty(),
        .canRedo = !redo.isEmpty(),
    };
}

const QVector<ShellLayout::LogicalOutput> &LayoutEditingRepository::outputs() const noexcept
{
    return m_outputs;
}

std::unique_ptr<LayoutEditingCoordinator>
LayoutEditingRepository::tryAcquireCoordinator()
{
    if (m_session->coordinatorAcquired) {
        return {};
    }
    auto coordinator = std::unique_ptr<LayoutEditingCoordinator>(
        new LayoutEditingCoordinator(*this));
    m_session->coordinatorAcquired = true;
    return coordinator;
}

void LayoutEditingRepository::publish(
    std::shared_ptr<const LayoutEditingSnapshot> snapshot) noexcept
{
    // AGENT-CONTRACT: Candidate profile and every output geometry are fully
    // validated before this no-fail pointer swap. Shell observers therefore
    // cannot observe one output from a rejected multi-output edit.
    m_session->snapshot.swap(snapshot);
}

void LayoutEditingRepository::releaseCoordinator() noexcept
{
    m_session->coordinatorAcquired = false;
}

} // namespace QindaQt::ShellCustomization
