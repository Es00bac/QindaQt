// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "desktopcontrolscomposition.h"
#include "desktopmenucomposition.h"
#include "gatheroverviewcomposition.h"
#include "globalmenuappletcomposition.h"
#include "launcherappletcomposition.h"
#include "livecustomizationcontroller.h"
#include "settingsroutelauncher.h"
#include "shortcutnotecontroller.h"
#include "wallpapercontroller.h"

#include "qindaqt/shell/desktop_controls/desktop_controls_access.h"
#include "qindaqt/shell/desktop_controls/places_controller.h"
#include "qindaqt/shell/desktop_controls/system_menu_controller.h"
#include "qindaqt/shell/desktop_menu/desktop_surface_commands.h"

#include <QPointer>

namespace QindaQt::Shell {

void ShellRuntimeApplication::initializeDesktopMenu(const Profiles::LayoutProfile &profile)
{
    if (!m_globalMenuApplet || !m_windowActionsClient) {
        return;
    }
    ShellDesktopMenuTargets::Controllers controllers;
    if (m_desktopControls != nullptr) {
        if (const auto *access = m_desktopControls->access()) {
            controllers.systemMenu =
                qobject_cast<DesktopControls::SystemMenuController *>(access->systemMenu());
            controllers.places =
                qobject_cast<DesktopControls::PlacesController *>(access->places());
        }
        controllers.workspaces = m_desktopControls->workspaces();
    }
    controllers.launcher = m_launcherApplet ? m_launcherApplet->access() : nullptr;
    controllers.clipboard = m_clipboardApplet ? m_clipboardApplet->access() : nullptr;
    controllers.desktopSurface = m_desktopSurfaceCommands.get();

    // AGENT-NOTE: runtime-only owners are lent as narrow callables so the
    // targets port never links their QML/KGlobalAccel machinery. Each owner
    // outlives the desktop menu (resetRuntime() destroys the menu first); the
    // note is additionally guarded because the wallpaper parents it.
    ShellDesktopMenuTargets::Hooks hooks;
    if (auto *routes = m_settingsRouteLauncher.get()) {
        hooks.openSettingsRoute = [routes](const QString &page, const QString &destination) {
            return routes->openRoute(page, destination);
        };
    }
    if (auto *overview = m_gatherOverview.get()) {
        hooks.toggleGatherOverview = [overview] { overview->toggle(); };
    }
    const QPointer<ShortcutNoteController> note =
        m_wallpaper ? m_wallpaper->shortcutNote() : nullptr;
    if (note) {
        hooks.shortcutNoteVisible = [note] { return note && note->noteVisible(); };
        hooks.toggleShortcutNote = [note] {
            if (note) {
                note->toggle();
            }
        };
    }
    // View > Edit Panels (ADR-0266): the live customization controller is
    // destroyed after the desktop menu (resetRuntime()).
    LiveCustomizationController *const panels = m_liveCustomization.get();
    if (panels != nullptr) {
        hooks.editingPanels = [panels] { return panels->editMode(); };
        hooks.toggleEditPanels = [panels] { panels->toggleEditMode(); };
    }

    m_desktopMenu = std::make_unique<DesktopMenuComposition>(DesktopMenuComposition::Borrowed{
        m_globalMenuApplet->access(), m_windowActionsClient.get(), controllers, std::move(hooks)});
    if (note && m_desktopMenu->targets() != nullptr) {
        connect(note.data(), &ShortcutNoteController::noteVisibleChanged, m_desktopMenu->targets(),
                &ShellDesktopMenuTargets::notifyFactsChanged);
    }
    if (panels != nullptr && m_desktopMenu->targets() != nullptr) {
        connect(panels, &LiveCustomizationController::editModeChanged, m_desktopMenu->targets(),
                &ShellDesktopMenuTargets::notifyFactsChanged);
    }
    followDesktopMenuLayout(profile);
}

void ShellRuntimeApplication::followDesktopMenuLayout(const Profiles::LayoutProfile &profile)
{
    if (!m_desktopMenu || !m_globalMenuApplet) {
        return;
    }
    // AGENT-CONTRACT (ADR-0130/0260): the desktop menu exists exactly while the
    // global menu is hosted and granted; a layout without a global menu keeps
    // menus in windows and shows no desktop menu either.
    m_desktopMenu->followLayout(
        m_globalMenuApplet->status() != GlobalMenuRuntimeStatus::Unavailable,
        DesktopMenuComposition::hostedApplets(profile, m_applets, m_appletPolicy));
}

} // namespace QindaQt::Shell
