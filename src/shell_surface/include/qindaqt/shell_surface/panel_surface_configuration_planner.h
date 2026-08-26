// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_layout/panel_layout_types.h"
#include "qindaqt/shell_surface/panel_surface_configuration.h"

namespace QindaQt::ShellSurface {

class PanelSurfaceConfigurationPlanner final {
public:
    // Converts a complete, solved logical layout into one deterministic set of
    // platform-surface values. The planner retains no state and returns no
    // partial configurations when any solver/result invariant is inconsistent.
    [[nodiscard]] static PanelSurfacePlan plan(
        const ShellLayout::PanelLayoutResult &layout);
};

} // namespace QindaQt::ShellSurface
