// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_configuration.h"

namespace QindaQt::ShellSurface {

struct PanelSurfaceRuntimeDecision {
    PanelSurfaceIdentity identity;
    PanelSurfaceMapping mapping = PanelSurfaceMapping::Mapped;
    bool reserve = true;

    friend bool operator==(const PanelSurfaceRuntimeDecision &,
                           const PanelSurfaceRuntimeDecision &) = default;
};

enum class PanelSurfaceRuntimePlanErrorCode {
    None,
    InvalidBasePlan,
    DuplicateBaseSurface,
    InvalidDecisionIdentity,
    InvalidDecisionState,
    DuplicateDecision,
    UnknownDecision,
    MissingDecision,
    HiddenSurfaceReservation,
    IneligibleSurfaceReservation,
    InvalidSurface,
    ArithmeticOverflow,
};

struct PanelSurfaceRuntimePlanError {
    PanelSurfaceRuntimePlanErrorCode code = PanelSurfaceRuntimePlanErrorCode::None;
    PanelSurfaceIdentity identity;
    QString message;

    [[nodiscard]] bool hasError() const noexcept
    {
        return code != PanelSurfaceRuntimePlanErrorCode::None;
    }
};

struct PanelSurfaceRuntimePlanResult {
    PanelSurfacePlan plan;
    PanelSurfaceRuntimePlanError error;

    [[nodiscard]] bool ok() const noexcept
    {
        return plan.ok() && !error.hasError();
    }
};

class PanelSurfaceRuntimePlanner final {
public:
    // Decisions must form an exact identity bijection with basePlan. The
    // resulting set preserves authored geometry while recalculating the one
    // effective reservation carrier on every output edge.
    [[nodiscard]] static PanelSurfaceRuntimePlanResult apply(
        const PanelSurfacePlan &basePlan,
        const QVector<PanelSurfaceRuntimeDecision> &decisions);
};

} // namespace QindaQt::ShellSurface
