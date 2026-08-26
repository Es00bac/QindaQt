// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Compositor::KWinIntegration {

// Platform-value projection kept independent of KWin headers so the
// visibility boundary can be qualified without constructing a compositor.
struct ShellVisibilityWindowAdmission final
{
    bool exists = false;
    bool deleted = false;
    bool internal = false;
    bool managed = false;
    bool desktop = false;
    bool dock = false;
    bool splash = false;
    bool tooltip = false;
    bool menu = false;
    bool popup = false;
    bool normal = false;
    bool dialog = false;
    bool utility = false;
    bool transient = false;
};

// Visibility observes ordinary managed user windows, not only windows eligible
// to become Hybrid leaves. In particular, a dialog or transient can obscure a
// panel and must participate in dodge policy.
[[nodiscard]] bool admitsShellVisibilityWindow(
    const ShellVisibilityWindowAdmission &window) noexcept;

} // namespace QindaQt::Compositor::KWinIntegration
