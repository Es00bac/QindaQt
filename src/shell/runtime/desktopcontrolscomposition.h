// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

#include <memory>
#include <optional>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}
namespace QindaQt::Applets {
class ManifestCatalog;
}
namespace QindaQt::Shell::Launcher {
class LauncherAppletController;
class LaunchSpawner;
class QProcessLaunchSpawner;
}
namespace QindaQt::Shell::GlobalMenu {
class GlobalMenuAppletAccess;
}
namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
}
namespace QindaQt::Shell::AudioApplet {
class AudioAppletController;
}
namespace QindaQt::Shell::BluetoothApplet {
class BluetoothAppletController;
}
namespace QindaQt::Shell::PowerApplet {
class PowerAppletController;
}
namespace QindaQt::Shell::Workspaces {
class WorkspaceController;
class WorkspaceTransport;
class QtWorkspaceTransport;
}
namespace QindaQt::Shell::DesktopControls {
class ActiveApplicationController;
class CommandSearchController;
class DesktopControlsAccess;
class FileManagerFolderOpener;
class FolderOpener;
class PlacesController;
class QuickLaunchController;
class SystemMenuController;
class SystemStatusController;
}

namespace QindaQt::Shell {

// Shell-private composition root for the twelve desktop controls. It
// evaluates each manifest's audited grants once, owns the authenticated
// workspace transport and the file-manager spawner, and exposes only the
// `DesktopControlsAccess` facade to the panel factory. Every other
// collaborator is BORROWED from the compositions that already own it.
//
// AGENT-CONTRACT: the borrowed facades must outlive this composition, and the
// panel windows must be destroyed before it (the existing resetRuntime order
// for the other compositions). The injected constructor keeps tests on the
// same seams without a bus or a process.
class DesktopControlsComposition final
{
public:
    struct BorrowedFacades {
        Launcher::LauncherAppletController *launcher = nullptr;
        GlobalMenu::GlobalMenuAppletAccess *globalMenu = nullptr;
        ShellTaskListApplet::TaskListAppletController *taskList = nullptr;
        AudioApplet::AudioAppletController *audio = nullptr;
        BluetoothApplet::BluetoothAppletController *bluetooth = nullptr;
        PowerApplet::PowerAppletController *power = nullptr;
        QObject *sessionActions = nullptr;
    };

    DesktopControlsComposition(const Applets::ManifestCatalog &catalog,
                               const AppletHost::CapabilityPolicy &policy,
                               const QDBusConnection &sessionBus,
                               std::optional<qint64> compositorProcessId,
                               BorrowedFacades facades);
    DesktopControlsComposition(const Applets::ManifestCatalog &catalog,
                               const AppletHost::CapabilityPolicy &policy,
                               Workspaces::WorkspaceTransport &workspaceTransport,
                               DesktopControls::FolderOpener &folderOpener,
                               BorrowedFacades facades);
    ~DesktopControlsComposition();

    DesktopControlsComposition(const DesktopControlsComposition &) = delete;
    DesktopControlsComposition &operator=(const DesktopControlsComposition &) = delete;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    [[nodiscard]] DesktopControls::DesktopControlsAccess *access() const noexcept;
    [[nodiscard]] Workspaces::WorkspaceController *workspaces() const noexcept;

private:
    void compose(const Applets::ManifestCatalog &catalog,
                 const AppletHost::CapabilityPolicy &policy,
                 Workspaces::WorkspaceTransport &workspaceTransport,
                 DesktopControls::FolderOpener &folderOpener,
                 const BorrowedFacades &facades);

    // AGENT-CONTRACT: reverse destruction is access -> controllers ->
    // owned opener/spawner -> owned transport. Borrowed facades are never
    // deleted here.
    std::unique_ptr<Workspaces::QtWorkspaceTransport> m_ownedTransport;
    Workspaces::WorkspaceTransport *m_transport = nullptr;
    std::unique_ptr<Launcher::QProcessLaunchSpawner> m_ownedSpawner;
    std::unique_ptr<DesktopControls::FileManagerFolderOpener> m_ownedOpener;
    std::unique_ptr<Workspaces::WorkspaceController> m_workspaces;
    std::unique_ptr<DesktopControls::SystemStatusController> m_systemStatus;
    std::unique_ptr<DesktopControls::SystemMenuController> m_systemMenu;
    std::unique_ptr<DesktopControls::PlacesController> m_places;
    std::unique_ptr<DesktopControls::QuickLaunchController> m_quickLaunch;
    std::unique_ptr<DesktopControls::ActiveApplicationController> m_activeApplication;
    std::unique_ptr<DesktopControls::CommandSearchController> m_commandPalette;
    std::unique_ptr<DesktopControls::CommandSearchController> m_commandHud;
    std::unique_ptr<DesktopControls::CommandSearchController> m_overview;
    std::unique_ptr<DesktopControls::DesktopControlsAccess> m_access;
    bool m_started = false;
};

} // namespace QindaQt::Shell
