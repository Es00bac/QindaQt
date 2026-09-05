// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopcontrolscomposition.h"

#include "launch_spawner.h"
#include "launcher_applet_controller.h"

#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/shell/desktop_controls/active_application_controller.h"
#include "qindaqt/shell/desktop_controls/applet_grants.h"
#include "qindaqt/shell/desktop_controls/command_search_controller.h"
#include "qindaqt/shell/desktop_controls/desktop_controls_access.h"
#include "qindaqt/shell/desktop_controls/file_manager_folder_opener.h"
#include "qindaqt/shell/desktop_controls/places_controller.h"
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"
#include "qindaqt/shell/desktop_controls/system_menu_controller.h"
#include "qindaqt/shell/desktop_controls/system_status_controller.h"
#include "qindaqt/shell/workspaces/qt_workspace_transport.h"
#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QCoreApplication>

namespace QindaQt::Shell {
namespace {

using Applets::Capability;
using DesktopControls::AuditedGrants;

} // namespace

DesktopControlsComposition::DesktopControlsComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    const QDBusConnection &sessionBus,
    std::optional<qint64> compositorProcessId,
    BorrowedFacades facades)
{
    Workspaces::WorkspaceAuthority authority;
    authority.compositorProcessId = compositorProcessId;
    m_ownedTransport = std::make_unique<Workspaces::QtWorkspaceTransport>(
        sessionBus, authority);
    // AGENT-CONTRACT: the file manager is started through the launcher's own
    // process seam (no shell, sanitized environment) with absolute program
    // candidates only; see ADR-0062 and FileManagerFolderOpener.
    m_ownedSpawner = std::make_unique<Launcher::QProcessLaunchSpawner>();
    m_ownedOpener = std::make_unique<DesktopControls::FileManagerFolderOpener>(
        *m_ownedSpawner,
        DesktopControls::FileManagerFolderOpener::defaultProgramCandidates());
    compose(catalog, policy, *m_ownedTransport, *m_ownedOpener, facades);
}

DesktopControlsComposition::DesktopControlsComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    Workspaces::WorkspaceTransport &workspaceTransport,
    DesktopControls::FolderOpener &folderOpener,
    BorrowedFacades facades)
{
    compose(catalog, policy, workspaceTransport, folderOpener, facades);
}

DesktopControlsComposition::~DesktopControlsComposition()
{
    stop();
}

void DesktopControlsComposition::compose(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    Workspaces::WorkspaceTransport &workspaceTransport,
    DesktopControls::FolderOpener &folderOpener,
    const BorrowedFacades &facades)
{
    m_transport = &workspaceTransport;
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    const auto grantsFor = [&](const char *manifestId) {
        return DesktopControls::evaluateAuditedGrants(
            catalog, policy, registry, QString::fromLatin1(manifestId));
    };

    // Workspace truth is shared by the switcher, tiles, show-desktop, palette,
    // overview, and dashboard. The union of their read/manage grants decides
    // the single controller; each consumer additionally carries its own grant.
    const AuditedGrants switcher = grantsFor("workspace-switcher");
    const AuditedGrants tiles = grantsFor("workspace-tiles");
    const AuditedGrants showDesktop = grantsFor("show-desktop");
    Workspaces::WorkspaceGrants workspaceGrants;
    workspaceGrants.windowsRead = switcher.has(Capability::WindowRead)
        || tiles.has(Capability::WindowRead) || showDesktop.has(Capability::WindowManage);
    workspaceGrants.windowsManage = switcher.has(Capability::WindowManage)
        || tiles.has(Capability::WindowManage) || showDesktop.has(Capability::WindowManage);
    if (workspaceGrants.windowsRead) {
        m_workspaces = std::make_unique<Workspaces::WorkspaceController>(
            m_transport, workspaceGrants);
    }

    const AuditedGrants status = grantsFor("system-status");
    if (status.resolved) {
        DesktopControls::SystemStatusGrants statusGrants;
        statusGrants.audioRead = status.has(Capability::AudioRead);
        statusGrants.audioControl = status.has(Capability::AudioControl);
        statusGrants.bluetoothRead = status.has(Capability::BluetoothRead);
        statusGrants.bluetoothControl = status.has(Capability::BluetoothControl);
        statusGrants.powerRead = status.has(Capability::PowerRead);
        statusGrants.powerControl = status.has(Capability::PowerControl);
        m_systemStatus = std::make_unique<DesktopControls::SystemStatusController>(
            facades.audio, facades.bluetooth, facades.power, statusGrants);
    }

    const AuditedGrants systemMenu = grantsFor("system-menu");
    if (systemMenu.resolved) {
        m_systemMenu = std::make_unique<DesktopControls::SystemMenuController>(
            facades.sessionActions, facades.launcher,
            systemMenu.has(Capability::ApplicationLaunch),
            DesktopControls::SystemMenuController::defaultSettingsEntryId(),
            QCoreApplication::applicationVersion());
    }

    const AuditedGrants places = grantsFor("places-menu");
    if (places.resolved) {
        m_places = std::make_unique<DesktopControls::PlacesController>(
            &folderOpener, places.has(Capability::ApplicationLaunch),
            DesktopControls::standardPlaces());
    }

    const AuditedGrants quickLaunch = grantsFor("quick-launch");
    if (quickLaunch.resolved) {
        m_quickLaunch = std::make_unique<DesktopControls::QuickLaunchController>(
            facades.launcher, quickLaunch.has(Capability::ApplicationLaunch));
    }

    const AuditedGrants activeApplication = grantsFor("active-application");
    if (activeApplication.resolved) {
        m_activeApplication =
            std::make_unique<DesktopControls::ActiveApplicationController>(
                facades.taskList,
                DesktopControls::ActiveApplicationGrants{
                    activeApplication.has(Capability::WindowRead),
                    activeApplication.has(Capability::WindowManage)});
    }

    const DesktopControls::CommandSearchController::Sources sources{
        facades.launcher, facades.globalMenu, facades.taskList, m_workspaces.get()};
    const auto searchGrants = [](const AuditedGrants &grants) {
        DesktopControls::CommandSearchController::Grants result;
        result.applicationsLaunch = grants.has(Capability::ApplicationLaunch);
        result.globalMenuRead = grants.has(Capability::GlobalMenuRead);
        result.windowsRead = grants.has(Capability::WindowRead);
        result.windowsActivate = grants.has(Capability::WindowActivate);
        result.windowsManage = grants.has(Capability::WindowManage);
        return result;
    };
    using DesktopControls::CommandSourceKind;
    const AuditedGrants palette = grantsFor("command-palette");
    if (palette.resolved) {
        m_commandPalette = std::make_unique<DesktopControls::CommandSearchController>(
            sources, searchGrants(palette),
            QList<CommandSourceKind>{CommandSourceKind::Applications,
                                     CommandSourceKind::MenuActions,
                                     CommandSourceKind::Windows,
                                     CommandSourceKind::Workspaces});
    }
    const AuditedGrants hud = grantsFor("command-hud");
    if (hud.resolved) {
        m_commandHud = std::make_unique<DesktopControls::CommandSearchController>(
            sources, searchGrants(hud),
            QList<CommandSourceKind>{CommandSourceKind::MenuActions});
    }
    const AuditedGrants overview = grantsFor("overview-trigger");
    if (overview.resolved) {
        m_overview = std::make_unique<DesktopControls::CommandSearchController>(
            sources, searchGrants(overview),
            QList<CommandSourceKind>{CommandSourceKind::Windows,
                                     CommandSourceKind::Workspaces,
                                     CommandSourceKind::Applications});
    }

    const AuditedGrants dashboard = grantsFor("dashboard");
    DesktopControls::DesktopControlsAccess::Facades exposed;
    exposed.workspaces = m_workspaces.get();
    exposed.systemStatus = m_systemStatus.get();
    exposed.systemMenu = m_systemMenu.get();
    exposed.places = m_places.get();
    exposed.quickLaunch = m_quickLaunch.get();
    exposed.activeApplication = m_activeApplication.get();
    exposed.commandPalette = m_commandPalette.get();
    exposed.commandHud = m_commandHud.get();
    exposed.overview = m_overview.get();
    // The dashboard borrows the launcher only when its own manifest grants
    // application launching; otherwise it lists no applications.
    exposed.launcher = dashboard.has(Capability::ApplicationLaunch) ? facades.launcher
                                                                     : nullptr;
    m_access = std::make_unique<DesktopControls::DesktopControlsAccess>(exposed);
}

bool DesktopControlsComposition::start(QString *error)
{
    if (m_started) {
        return true;
    }
    m_started = true;
    if (m_workspaces && m_transport != nullptr) {
        return m_transport->start(error);
    }
    return true;
}

void DesktopControlsComposition::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    if (m_transport != nullptr) {
        m_transport->stop();
    }
}

DesktopControls::DesktopControlsAccess *DesktopControlsComposition::access() const noexcept
{
    return m_access.get();
}

Workspaces::WorkspaceController *DesktopControlsComposition::workspaces() const noexcept
{
    return m_workspaces.get();
}

} // namespace QindaQt::Shell
