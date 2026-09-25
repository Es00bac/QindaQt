// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "installer_planning.h"
#include "launch_planner.h"

#include <functional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the composition root's adapter from the install jobs'
// InstallerPlanner seam to umu's rules (ADR-0275 section 1). It lives here,
// not in the model, because the model never depends on the jobs library.
//
// An installer runs exactly like a game: `umu-run <windowsCommand...>` with
// the five umu variables from umuRunEnvironment (written last) and the same
// reserved set unset (umuUnsetEnvironmentKeys/Prefixes). The build must be
// the catalog entry whose name AND absolute directory match the request,
// pinnable, and still present on disk; anything else is refused with a
// plain sentence -- never a different build.
[[nodiscard]] InstallerPlan planUmuInstallerRun(const InstallerPlanRequest &request,
                                                const LaunchToolSet &tools);

// A planner bound to the current tool set. `tools` is called for every
// plan, so a refresh that re-discovers builds is honoured by the next run.
[[nodiscard]] InstallerPlanner makeUmuInstallerPlanner(
    std::function<LaunchToolSet()> tools);

} // namespace QindaQt::QindaLutris
