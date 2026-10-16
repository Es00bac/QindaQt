// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Shell::DesktopControls {

// AGENT-CONTRACT: the target id of the Show/Hide desktop toggle among the
// workspace candidates. command_search_collectors.cpp produces it and
// CommandSearchController::activateCandidate consumes it, so the literal lives
// in this one private header instead of being repeated in both.
inline constexpr auto ShowDesktopTarget = "show-desktop";

} // namespace QindaQt::Shell::DesktopControls
