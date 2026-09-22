// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization/layout_editing_repository.h"

#include "layout_candidate_validator_p.h"
#include "layout_editing_repository_p.h"
#include "qindaqt/shell_customization/layout_editing_coordinator.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"

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

    // AGENT-GUARD: park the panels this generation cannot host before the
    // solve, never after. A single panel pinned to an unplugged display
    // otherwise fails the initial solve and leaves the whole repository
    // non-ready - which in the shell means every Meta+right-click entry point
    // goes dead until something happens to rebuild the session, and in
    // Settings means the Customize route will not open at all.
    QVector<Profiles::PanelSpec> placeable;
    placeable.reserve(initialProfile.panels.size());
    for (qsizetype index = 0; index < initialProfile.panels.size(); ++index) {
        const Profiles::PanelSpec &panel = initialProfile.panels.at(index);
        if (ShellLayout::PanelLayoutSolver::outputsCanHost(panel, m_outputs)) {
            placeable.append(panel);
        } else {
            m_session->escrowed.append({index, panel});
        }
    }
    initialProfile.panels = std::move(placeable);

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

const QVector<EscrowedPanel> &LayoutEditingRepository::escrowedPanels() const noexcept
{
    return m_session->escrowed;
}

Profiles::LayoutProfile LayoutEditingRepository::withEscrowedPanels(
    const Profiles::LayoutProfile &edited, const QVector<EscrowedPanel> &escrowed)
{
    if (escrowed.isEmpty()) {
        return edited;
    }
    Profiles::LayoutProfile result = edited;
    result.panels.clear();
    result.panels.reserve(edited.panels.size() + escrowed.size());

    // Walk the stored indices in order, emitting each escrowed panel back at
    // the slot it came from and the edited panels around it. An escrowed index
    // past the end simply lands at the end, which is what a session that
    // removed panels ahead of it should produce.
    qsizetype editedIndex = 0;
    qsizetype nextSlot = 0;
    for (const EscrowedPanel &parked : escrowed) {
        while (editedIndex < edited.panels.size() && nextSlot < parked.index) {
            result.panels.append(edited.panels.at(editedIndex));
            ++editedIndex;
            ++nextSlot;
        }
        result.panels.append(parked.panel);
        ++nextSlot;
    }
    while (editedIndex < edited.panels.size()) {
        result.panels.append(edited.panels.at(editedIndex));
        ++editedIndex;
    }
    return result;
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
