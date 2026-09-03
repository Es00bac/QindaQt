// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QProcessEnvironment>
#include <QStringList>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Shell::Launcher {
class ApplicationScanner;
class LaunchActivator;
class LaunchExecutor;
class LaunchSpawner;
class LauncherAppletController;
class LauncherPersistenceController;
class QProcessLaunchSpawner;
class SessionBusActivator;
}

namespace QindaQt::Shell {

// Resolves XDG application data roots from caller-supplied process facts.
// Invalid relative entries are ignored and order/first occurrence are kept.
[[nodiscard]] QStringList launcherDataRoots(
    const QProcessEnvironment &environment, const QString &homeDirectory);

// Shell-private lifetime boundary for installed-application scanning,
// Settings1 identity persistence, and bounded execution. Production owns the
// spawner/activator; focused tests inject recordings through the second
// constructor and never start an application.
class LauncherAppletComposition final
{
public:
    LauncherAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        QStringList dataRoots,
        Services::SettingsClient::SettingsClient &settingsClient,
        const QDBusConnection &sessionBus,
        QStringList terminalCommand = {});
    LauncherAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        QStringList dataRoots,
        Services::SettingsClient::SettingsClient &settingsClient,
        Launcher::LaunchSpawner &spawner,
        Launcher::LaunchActivator &activator,
        QStringList terminalCommand = {});
    ~LauncherAppletComposition();

    LauncherAppletComposition(const LauncherAppletComposition &) = delete;
    LauncherAppletComposition &operator=(const LauncherAppletComposition &) = delete;

    [[nodiscard]] bool start(QString *error = nullptr);
    [[nodiscard]] Launcher::LauncherAppletController *access() const noexcept;

private:
    void compose(const Applets::ManifestCatalog &catalog,
                 const AppletHost::CapabilityPolicy &policy,
                 QStringList dataRoots,
                 Services::SettingsClient::SettingsClient &settingsClient,
                 Launcher::LaunchSpawner &spawner,
                 Launcher::LaunchActivator &activator,
                 QStringList terminalCommand);

    // AGENT-CONTRACT: reverse destruction is access -> executor -> persistence
    // -> scanner -> activator -> spawner. The borrowed Settings1 client and
    // injected seams must outlive this object; ShellRuntimeApplication tears
    // panel windows down before destroying this composition.
    std::unique_ptr<Launcher::QProcessLaunchSpawner> m_ownedSpawner;
    std::unique_ptr<Launcher::SessionBusActivator> m_ownedActivator;
    std::unique_ptr<Launcher::ApplicationScanner> m_scanner;
    std::unique_ptr<Launcher::LauncherPersistenceController> m_persistence;
    std::unique_ptr<Launcher::LaunchExecutor> m_executor;
    std::unique_ptr<Launcher::LauncherAppletController> m_access;
};

} // namespace QindaQt::Shell
