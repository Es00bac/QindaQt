// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/windows_runner/launch_types.h>

#include <optional>

namespace QindaQt::WindowsRunner {

struct WindowsLaunchPreparation final {
    std::optional<WindowsLaunchPlan> plan;
    LaunchFailure failure = LaunchFailure::None;
};

// Pure validation/planning over copied admitted values, no filesystem/spawn.
// sessionEnvironment is an injected current-session snapshot; only explicit
// display, locale and audio transport fields survive. Profile and runtime
// variables are owned by the recipe, not by desktop text or ambient overrides.
[[nodiscard]] WindowsLaunchPreparation prepareWindowsLaunch(
    const ManagedWindowsBinding &binding,
    const RegisteredWindowsApplication &application,
    const QProcessEnvironment &sessionEnvironment);

} // namespace QindaQt::WindowsRunner
