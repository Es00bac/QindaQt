// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_orchestration/panel_runtime_plan_assembler.h"

#include <utility>

namespace QindaQt::ShellOrchestration {
namespace {

PanelRuntimeAssemblyResult apply(
    const ShellSurface::PanelSurfacePlan &basePlan,
    QVector<ShellSurface::PanelSurfaceRuntimeDecision> decisions)
{
    auto runtime = ShellSurface::PanelSurfaceRuntimePlanner::apply(basePlan, decisions);
    if (!runtime.ok()) {
        return {{},
                {PanelRuntimeAssemblyErrorCode::RuntimePlanRejected,
                 runtime.error.message},
                std::move(runtime.error)};
    }
    return {std::move(runtime.plan), {}, {}};
}

} // namespace

PanelRuntimeAssemblyResult PanelRuntimePlanAssembler::safeVisible(
    const ShellSurface::PanelSurfacePlan &basePlan)
{
    QVector<ShellSurface::PanelSurfaceRuntimeDecision> decisions;
    decisions.reserve(basePlan.surfaces.size());
    for (const auto &surface : basePlan.surfaces) {
        decisions.append({surface.identity,
                          ShellSurface::PanelSurfaceMapping::Mapped,
                          surface.reservesWorkArea});
    }
    return apply(basePlan, std::move(decisions));
}

PanelRuntimeAssemblyResult PanelRuntimePlanAssembler::fromEvaluation(
    const ShellSurface::PanelSurfacePlan &basePlan,
    const ShellVisibility::PanelVisibilityEvaluation &evaluation)
{
    if (!evaluation.ok()) {
        return {{},
                {PanelRuntimeAssemblyErrorCode::RejectedVisibilityEvaluation,
                 evaluation.error.message},
                {}};
    }
    QVector<ShellSurface::PanelSurfaceRuntimeDecision> decisions;
    decisions.reserve(evaluation.decisions.size());
    for (const auto &decision : evaluation.decisions) {
        decisions.append({
            {decision.identity.panelId, decision.identity.outputId},
            decision.visibility == ShellVisibility::PanelVisibility::Visible
                ? ShellSurface::PanelSurfaceMapping::Mapped
                : ShellSurface::PanelSurfaceMapping::Unmapped,
            decision.reservation == ShellVisibility::PanelReservationIntent::Reserve,
        });
    }
    return apply(basePlan, std::move(decisions));
}

} // namespace QindaQt::ShellOrchestration
