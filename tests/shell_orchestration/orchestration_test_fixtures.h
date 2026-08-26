// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"
#include "qindaqt/shell_surface/panel_surface_configuration_planner.h"
#include "qindaqt/shell_visibility/compositor_visibility_snapshot.h"

namespace QindaQt::ShellOrchestration::TestFixtures {

inline Profiles::PanelSpec panel(QString id = QStringLiteral("main"))
{
    Profiles::PanelSpec result;
    result.id = std::move(id);
    result.applets = {{.id = QStringLiteral("clock-instance"),
                       .plugin = QStringLiteral("clock"),
                       .settings = {}}};
    return result;
}

inline Profiles::LayoutProfile profile(Profiles::PanelSpec panelSpec = panel())
{
    Profiles::LayoutProfile result;
    result.id = QStringLiteral("fixture");
    result.name = QStringLiteral("Fixture");
    result.panels = {std::move(panelSpec)};
    return result;
}

inline QVector<ShellLayout::LogicalOutput> outputs()
{
    return {
        {QStringLiteral("left"), {-1600, 0, 1600, 900}, 1.0},
        {QStringLiteral("main"), {0, 0, 2560, 1440}, 1.5},
    };
}

inline ShellVisibility::CompositorVisibilitySnapshot compositor(
    const QVector<ShellLayout::LogicalOutput> &logicalOutputs = outputs())
{
    ShellVisibility::CompositorVisibilitySnapshot result;
    result.epoch = QStringLiteral("00000000-0000-4000-8000-000000000001");
    result.revision = 1;
    result.scope = {QStringLiteral("workspace-1"), QStringLiteral("activity-1")};
    for (const auto &output : logicalOutputs) {
        result.outputs.append({output.id, output.geometry, output.scale});
    }
    return result;
}

inline ShellLayout::PanelLayoutResult layout(
    const Profiles::LayoutProfile &layoutProfile,
    const QVector<ShellLayout::LogicalOutput> &logicalOutputs = outputs())
{
    return ShellLayout::PanelLayoutSolver::solve(layoutProfile.panels, logicalOutputs);
}

inline ShellSurface::PanelSurfacePlan surfacePlan(
    const Profiles::LayoutProfile &layoutProfile,
    const QVector<ShellLayout::LogicalOutput> &logicalOutputs = outputs())
{
    return ShellSurface::PanelSurfaceConfigurationPlanner::plan(
        layout(layoutProfile, logicalOutputs));
}

} // namespace QindaQt::ShellOrchestration::TestFixtures
