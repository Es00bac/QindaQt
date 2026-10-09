// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/windows_runner/catalog_binding.h>

namespace QindaQt::WindowsRunner {
std::optional<ManagedWindowsBinding> windowsBinding(
    const ShellLauncher::ManagedApplicationBinding &binding)
{
    if (binding.origin != ShellLauncher::ApplicationOrigin::Windows
        || !ShellLauncher::isValidManagedApplicationBinding(binding)) {
        return std::nullopt;
    }
    return ManagedWindowsBinding{binding.registrationId,
        binding.registrationRevision, binding.recipeDigest};
}

WindowsLaunchResult launchManagedWindowsApplication(
    WindowsApplicationRunner &runner,
    const ShellLauncher::ManagedApplicationBinding &binding,
    const QProcessEnvironment &sessionEnvironment)
{
    const auto admitted = windowsBinding(binding);
    if (!admitted) {
        return {LaunchState::Unavailable, LaunchFailure::InvalidBinding, {}, {},
                QStringLiteral("This entry is not a valid Windows application registration.")};
    }
    return runner.launch(*admitted, sessionEnvironment);
}
} // namespace QindaQt::WindowsRunner
