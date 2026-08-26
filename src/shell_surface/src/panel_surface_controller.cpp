// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/panel_surface_controller.h"

#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"

#include <utility>

namespace QindaQt::ShellSurface {
namespace {

PanelSurfaceControllerResult failure(PanelSurfaceControllerErrorCode code,
                                     quint64 revision, QString message)
{
    return {code, revision, false, std::move(message)};
}

QString diagnosticOr(QString diagnostic, const QString &fallback)
{
    return diagnostic.trimmed().isEmpty() ? fallback : diagnostic;
}

} // namespace

PanelSurfaceController::PanelSurfaceController(PanelSurfaceBackend &backend,
                                               quint64 initialRevision)
    : m_backend(backend)
    , m_revision(initialRevision)
{
}

PanelSurfaceController::~PanelSurfaceController() = default;

PanelSurfaceControllerResult PanelSurfaceController::reconcile(
    const ShellLayout::PanelLayoutResult &layout)
{
    return reconcilePlan(PanelSurfaceConfigurationPlanner::plan(layout));
}

PanelSurfaceControllerResult PanelSurfaceController::reconcilePlan(
    PanelSurfacePlan candidate)
{
    if (!candidate.ok()) {
        return failure(PanelSurfaceControllerErrorCode::PlanningFailed, m_revision,
                       candidate.error.message);
    }
    const bool publishedSetIsLive = m_published && m_published->isLive();
    if (publishedSetIsLive && candidate == m_plan) {
        return {PanelSurfaceControllerErrorCode::None, m_revision, false, {}};
    }
    if (m_revision == std::numeric_limits<quint64>::max()) {
        return failure(PanelSurfaceControllerErrorCode::RevisionExhausted, m_revision,
                       QStringLiteral("panel surface revision is exhausted"));
    }

    if (publishedSetIsLive) {
        const auto reconfigured = m_published->reconfigure(candidate.surfaces);
        if (reconfigured.code == PublishedSurfaceReconfigureCode::Applied) {
            m_plan = std::move(candidate);
            ++m_revision;
            return {PanelSurfaceControllerErrorCode::None, m_revision, true, {}};
        }
        if (reconfigured.code == PublishedSurfaceReconfigureCode::Failed) {
            return failure(
                PanelSurfaceControllerErrorCode::BackendReconfigureFailed, m_revision,
                diagnosticOr(reconfigured.message,
                             QStringLiteral("panel surface backend could not reconfigure"
                                            " the live layout")));
        }
    }

    QString diagnostic;
    auto prepared = m_backend.prepare(candidate.surfaces, &diagnostic);
    if (!prepared) {
        return failure(
            PanelSurfaceControllerErrorCode::BackendPrepareFailed, m_revision,
            diagnosticOr(std::move(diagnostic),
                         QStringLiteral("panel surface backend could not prepare the layout")));
    }

    diagnostic.clear();
    auto published = prepared->publish(&diagnostic);
    if (!published) {
        return failure(
            PanelSurfaceControllerErrorCode::BackendPublishFailed, m_revision,
            diagnosticOr(std::move(diagnostic),
                         QStringLiteral("panel surface backend could not publish the layout")));
    }

    // AGENT-GUARD: The replacement is already published before this assignment
    // destroys the former set. Preparation/publication failures above therefore
    // preserve the old windows, plan, and revision exactly.
    m_published = std::move(published);
    m_plan = std::move(candidate);
    ++m_revision;
    return {PanelSurfaceControllerErrorCode::None, m_revision, true, {}};
}

quint64 PanelSurfaceController::revision() const noexcept
{
    return m_revision;
}

const PanelSurfacePlan &PanelSurfaceController::currentPlan() const noexcept
{
    return m_plan;
}

bool PanelSurfaceController::hasPublishedSet() const noexcept
{
    return m_published != nullptr;
}

} // namespace QindaQt::ShellSurface
