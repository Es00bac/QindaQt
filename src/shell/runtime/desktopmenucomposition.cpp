// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopmenucomposition.h"

#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell/global_menu/applet/globalmenuappletaccess.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include <utility>

namespace QindaQt::Shell {
namespace {

using Presence = DesktopMenu::DesktopMenuController::Presence;

[[nodiscard]] bool layoutHostsReadyApplet(const Profiles::LayoutProfile &profile,
                                          const Applets::ManifestCatalog &catalog,
                                          const AppletHost::CapabilityPolicy &policy,
                                          QLatin1StringView plugin)
{
    // An instance rejected by placement, host, implementation, or policy never
    // renders, so it can never answer an open request either.
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    for (const auto &panel : profile.panels) {
        for (const auto &applet : panel.applets) {
            if (applet.plugin == plugin && AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                                               applet, panel.edge, catalog, policy, registry)
                                               .ready()) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

DesktopMenuComposition::DesktopMenuComposition(Borrowed borrowed)
{
    if (borrowed.facade == nullptr || borrowed.windowActions == nullptr) {
        return;
    }
    m_targets =
        std::make_unique<ShellDesktopMenuTargets>(borrowed.controllers, std::move(borrowed.hooks));
    m_controller =
        std::make_unique<DesktopMenu::DesktopMenuController>(*borrowed.facade, *m_targets);
    auto *client = borrowed.windowActions;
    auto *controller = m_controller.get();
    m_identityConnection = QObject::connect(
        client, &ShellWindowActionsClient::ShellWindowActionsClient::identityChanged, controller,
        [client, controller] { controller->setPresence(presenceFrom(*client)); });
    controller->setPresence(presenceFrom(*client));
}

DesktopMenuComposition::~DesktopMenuComposition()
{
    QObject::disconnect(m_identityConnection);
    // The controller withdraws the desktop channel from the borrowed facade.
    m_controller.reset();
    m_targets.reset();
}

void DesktopMenuComposition::followLayout(bool globalMenuHosted,
                                          ShellDesktopMenuTargets::HostedApplets hosted)
{
    if (!m_controller) {
        return;
    }
    m_targets->setHostedApplets(hosted);
    m_controller->setEnabled(globalMenuHosted);
}

ShellDesktopMenuTargets::HostedApplets
DesktopMenuComposition::hostedApplets(const Profiles::LayoutProfile &profile,
                                      const Applets::ManifestCatalog &catalog,
                                      const AppletHost::CapabilityPolicy &policy)
{
    return {.launcher =
                layoutHostsReadyApplet(profile, catalog, policy, QLatin1StringView("launcher")),
            .clipboard =
                layoutHostsReadyApplet(profile, catalog, policy, QLatin1StringView("clipboard"))};
}

Presence DesktopMenuComposition::presenceFrom(
    const ShellWindowActionsClient::ShellWindowActionsClient &client)
{
    const auto &snapshot = client.identitySnapshot();
    if (!client.identityAvailable() || !snapshot || !snapshot->available() ||
        snapshot->revision == 0) {
        return Presence::Unknown;
    }
    return snapshot->activeWindow ? Presence::ApplicationActive : Presence::NoApplication;
}

DesktopMenu::DesktopMenuController *DesktopMenuComposition::controller() const noexcept
{
    return m_controller.get();
}

ShellDesktopMenuTargets *DesktopMenuComposition::targets() const noexcept
{
    return m_targets.get();
}

} // namespace QindaQt::Shell
