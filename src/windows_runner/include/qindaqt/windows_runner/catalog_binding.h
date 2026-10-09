// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/shell_launcher/managed_applications.h>
#include <qindaqt/windows_runner/windows_runner.h>

#include <optional>

namespace QindaQt::WindowsRunner {
[[nodiscard]] std::optional<ManagedWindowsBinding> windowsBinding(
    const ShellLauncher::ManagedApplicationBinding &binding);
// Typed origin admission followed by the runner's current-recipe/payload gate.
// This bridge does not turn a title, desktop Exec or extension into authority.
[[nodiscard]] WindowsLaunchResult launchManagedWindowsApplication(
    WindowsApplicationRunner &runner,
    const ShellLauncher::ManagedApplicationBinding &binding,
    const QProcessEnvironment &sessionEnvironment = {});
} // namespace QindaQt::WindowsRunner
