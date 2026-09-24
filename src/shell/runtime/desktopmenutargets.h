// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_menu/desktop_menu_targets.h"
#include "qindaqt/shell/desktop_menu/desktop_surface_commands.h"

#include <QString>

#include <functional>

namespace QindaQt::Shell::DesktopControls {
class PlacesController;
class SystemMenuController;
} // namespace QindaQt::Shell::DesktopControls
namespace QindaQt::Shell::Launcher {
class LauncherAppletController;
}
namespace QindaQt::Shell::Workspaces {
class WorkspaceController;
}
namespace QindaQt::ShellClipboardApplet {
class ClipboardAppletController;
}

namespace QindaQt::Shell {

// Shell-private implementation of the desktop menu's targets port
// (ADR-0260). Each command reaches the ONE existing controller that already
// owns it, through that controller's public boundary: the system menu
// controller (System Settings, and its session-actions facade for lock, log
// out, suspend, restart, shut down), the places controller (File Manager
// folders through the launcher's bounded process seam), the launcher
// (desktop-entry activation and its popup request), the clipboard applet's
// popup request, the workspace controller, the Settings route launcher, the
// gather overview's one door, the shortcut note, panel edit mode, and the
// desktop-icons surface commands. No command runs a program, a path, or a URL
// of its own.
//
// AGENT-CONTRACT: every controller and hook is borrowed, may be absent
// (null/empty: that capability is not present), and must outlive this object
// on the GUI thread. Session actions are reached exactly as the system menu
// applet's QML reaches them: the Q_PROPERTY/Q_INVOKABLE surface of the
// QObject SystemMenuController::sessionActions() lends out.
class ShellDesktopMenuTargets final : public DesktopMenu::DesktopMenuTargets {
    Q_OBJECT

public:
    struct Controllers {
        DesktopControls::SystemMenuController *systemMenu = nullptr;
        DesktopControls::PlacesController *places = nullptr;
        Workspaces::WorkspaceController *workspaces = nullptr;
        Launcher::LauncherAppletController *launcher = nullptr;
        ShellClipboardApplet::ClipboardAppletController *clipboard = nullptr;
        DesktopMenu::DesktopSurfaceCommands *desktopSurface = nullptr;
    };

    // Runtime-only owners reached through narrow callables, so this port does
    // not link their QML and KGlobalAccel machinery; tests inject recorders.
    struct Hooks {
        // SettingsRouteLauncher::openRoute(page, destination)
        std::function<bool(const QString &page, const QString &destination)> openSettingsRoute;
        // GatherOverviewComposition::toggle()
        std::function<void()> toggleGatherOverview;
        // ShortcutNoteController::noteVisible() / toggle()
        std::function<bool()> shortcutNoteVisible;
        std::function<void()> toggleShortcutNote;
        // LiveCustomizationController::editMode() / toggleEditMode()
        // (ADR-0266); the owner calls notifyFactsChanged() on editModeChanged.
        std::function<bool()> editingPanels;
        std::function<void()> toggleEditPanels;
    };

    // AGENT-NOTE: the Welcome application's desktop entry
    // (src/apps/welcome/org.qindaqt.Welcome.desktop) is QindaQt's help.
    [[nodiscard]] static QString helpDesktopEntryId();

    // Request-only popups (launcher search, clipboard history) are offered
    // only while the adopted layout hosts a ready renderer that answers them.
    struct HostedApplets {
        bool launcher = false;
        bool clipboard = false;

        friend bool operator==(const HostedApplets &, const HostedApplets &) = default;
    };

    ShellDesktopMenuTargets(Controllers controllers, Hooks hooks, QObject *parent = nullptr);

    void setHostedApplets(HostedApplets hosted);

    [[nodiscard]] DesktopMenu::DesktopMenuFacts facts() const override;
    bool perform(const DesktopMenu::DesktopMenuCommand &command) override;
    [[nodiscard]] QString lastFailure() const override { return m_lastFailure; }

public Q_SLOTS:
    // For owners without a typed change signal of their own (the shortcut
    // note, the session-actions facade).
    void notifyFactsChanged();

private:
    [[nodiscard]] QObject *sessionActions() const;
    bool invokeSession(const char *method, const QString &unavailable);
    bool openRoute(const QString &page, const QString &destination);
    bool activateEntry(const QString &entryId, const QString &unavailable);
    bool requestSurface(DesktopMenu::DesktopSurfaceCommands::Command command);
    bool accept();
    bool refuse(const QString &message);

    Controllers m_controllers;
    Hooks m_hooks;
    HostedApplets m_hosted;
    QString m_lastFailure;
};

} // namespace QindaQt::Shell
