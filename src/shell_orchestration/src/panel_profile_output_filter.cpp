// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_orchestration/panel_profile_output_filter.h"

#include "qindaqt/shell_layout/panel_layout_solver.h"

namespace QindaQt::ShellOrchestration {

Profiles::LayoutProfile PanelProfileOutputOccupancy::presentOutputsOnly(
    const Profiles::LayoutProfile &profile,
    const QVector<ShellLayout::LogicalOutput> &outputs)
{
    Profiles::LayoutProfile result = profile;
    result.panels.clear();
    result.panels.reserve(profile.panels.size());
    for (const auto &panel : profile.panels) {
        // AGENT-GUARD: ask the solver, never re-derive the match. A filter that
        // disagrees with solve() about which panels this generation can host is
        // exactly how an absent output used to take every other panel with it.
        if (ShellLayout::PanelLayoutSolver::outputsCanHost(panel, outputs)) {
            result.panels.append(panel);
        }
    }
    return result;
}

} // namespace QindaQt::ShellOrchestration
