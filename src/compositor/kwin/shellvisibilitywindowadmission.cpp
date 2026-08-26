// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellvisibilitywindowadmission.h"

namespace QindaQt::Compositor::KWinIntegration {

bool admitsShellVisibilityWindow(
    const ShellVisibilityWindowAdmission &window) noexcept
{
    const bool ordinaryUserWindow = window.normal || window.dialog || window.utility;
    return window.exists && window.managed && !window.deleted && !window.internal
        && !window.desktop && !window.dock && !window.splash && !window.tooltip
        && !window.menu && !window.popup && ordinaryUserWindow;
}

} // namespace QindaQt::Compositor::KWinIntegration
