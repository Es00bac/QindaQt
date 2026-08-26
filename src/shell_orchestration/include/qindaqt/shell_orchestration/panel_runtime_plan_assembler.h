// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_runtime_planner.h"
#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QString>

namespace QindaQt::ShellOrchestration {

enum class PanelRuntimeAssemblyErrorCode {
    None,
    RejectedVisibilityEvaluation,
    RuntimePlanRejected,
};

struct PanelRuntimeAssemblyError {
    PanelRuntimeAssemblyErrorCode code = PanelRuntimeAssemblyErrorCode::None;
    QString message;
};

struct PanelRuntimeAssemblyResult {
    ShellSurface::PanelSurfacePlan plan;
    PanelRuntimeAssemblyError error;
    ShellSurface::PanelSurfaceRuntimePlanError runtimeError;

    [[nodiscard]] bool ok() const noexcept
    {
        return plan.ok() &&
            error.code == PanelRuntimeAssemblyErrorCode::None &&
            !runtimeError.hasError();
    }
};

class PanelRuntimePlanAssembler final {
public:
    [[nodiscard]] static PanelRuntimeAssemblyResult safeVisible(
        const ShellSurface::PanelSurfacePlan &basePlan);
    [[nodiscard]] static PanelRuntimeAssemblyResult fromEvaluation(
        const ShellSurface::PanelSurfacePlan &basePlan,
        const ShellVisibility::PanelVisibilityEvaluation &evaluation);
};

} // namespace QindaQt::ShellOrchestration
