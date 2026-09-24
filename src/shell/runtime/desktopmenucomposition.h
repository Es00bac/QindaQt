// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "desktopmenutargets.h"

#include "qindaqt/shell/desktop_menu/desktop_menu_controller.h"

#include <QMetaObject>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}
namespace QindaQt::Applets {
class ManifestCatalog;
}
namespace QindaQt::Profiles {
class LayoutProfile;
}
namespace QindaQt::Shell::GlobalMenu {
class GlobalMenuAppletAccess;
}
namespace QindaQt::ShellWindowActionsClient {
class ShellWindowActionsClient;
}

namespace QindaQt::Shell {

// Shell-private composition root for the desktop menu (ADR-0260): owns the
// targets port over the borrowed controllers and the desktop menu
// controller, feeds the controller's presence from the shell's ONE
// exact-owner window-actions client (no second compositor connection), and
// follows the global menu's layout residency (ADR-0130).
//
// AGENT-CONTRACT: every borrowed object must outlive this composition;
// ShellRuntimeApplication::resetRuntime() destroys it first, before the
// wallpaper (shortcut note), desktop surface, desktop controls, launcher,
// clipboard, gather overview, global menu, and window-actions client. With a
// null facade or client it composes nothing and stays inert.
class DesktopMenuComposition final {
public:
    struct Borrowed {
        GlobalMenu::GlobalMenuAppletAccess *facade = nullptr;
        ShellWindowActionsClient::ShellWindowActionsClient *windowActions = nullptr;
        ShellDesktopMenuTargets::Controllers controllers;
        ShellDesktopMenuTargets::Hooks hooks;
    };

    explicit DesktopMenuComposition(Borrowed borrowed);
    ~DesktopMenuComposition();

    DesktopMenuComposition(const DesktopMenuComposition &) = delete;
    DesktopMenuComposition &operator=(const DesktopMenuComposition &) = delete;

    // Startup and every live layout adoption, after the global menu followed
    // the same layout: `globalMenuHosted` is true while it resolved a granted,
    // hosted global menu (Ready or Degraded), never for ADR-0130 layouts.
    void followLayout(bool globalMenuHosted, ShellDesktopMenuTargets::HostedApplets hosted);

    // Which request-only popups the adopted layout renders, resolved through
    // the same AppletInstanceResolver path the panel dispatcher uses.
    [[nodiscard]] static ShellDesktopMenuTargets::HostedApplets
    hostedApplets(const Profiles::LayoutProfile &profile, const Applets::ManifestCatalog &catalog,
                  const AppletHost::CapabilityPolicy &policy);

    // Compositor-authenticated presence: the compositor admits only ordinary
    // application windows as active, so the desktop surface (and shell
    // surfaces) being active reads as NoApplication. An unavailable, unproven,
    // or rereading identity is Unknown, never NoApplication.
    [[nodiscard]] static DesktopMenu::DesktopMenuController::Presence
    presenceFrom(const ShellWindowActionsClient::ShellWindowActionsClient &client);

    [[nodiscard]] DesktopMenu::DesktopMenuController *controller() const noexcept;
    [[nodiscard]] ShellDesktopMenuTargets *targets() const noexcept;

private:
    std::unique_ptr<ShellDesktopMenuTargets> m_targets;
    std::unique_ptr<DesktopMenu::DesktopMenuController> m_controller;
    QMetaObject::Connection m_identityConnection;
};

} // namespace QindaQt::Shell
