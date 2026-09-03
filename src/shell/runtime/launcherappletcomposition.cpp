// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcherappletcomposition.h"

#include "application_scanner.h"
#include "launch_activator.h"
#include "launch_executor.h"
#include "launch_spawner.h"
#include "launcher_applet_controller.h"
#include "launcher_persistence.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <utility>

namespace QindaQt::Shell {
namespace {

void appendAbsoluteRoot(const QString &candidate, QStringList *roots,
                        QSet<QString> *seen)
{
    if (candidate.isEmpty() || !QFileInfo(candidate).isAbsolute()) {
        return;
    }
    const QString clean = QDir::cleanPath(candidate);
    if (!seen->contains(clean)) {
        seen->insert(clean);
        roots->append(clean);
    }
}

bool applicationsLaunchGranted(const Applets::ManifestCatalog &catalog,
                               const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("launcher"));
    if (manifest == nullptr) {
        return false;
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return false;
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return false;
    }
    for (const auto &decision : evaluated.decisions) {
        if (decision.capability == Applets::Capability::ApplicationLaunch) {
            return decision.granted();
        }
    }
    return false;
}

} // namespace

QStringList launcherDataRoots(const QProcessEnvironment &environment,
                              const QString &homeDirectory)
{
    QStringList roots;
    QSet<QString> seen;
    const QString dataHome = environment.value(QStringLiteral("XDG_DATA_HOME"));
    if (!dataHome.isEmpty() && QFileInfo(dataHome).isAbsolute()) {
        appendAbsoluteRoot(dataHome, &roots, &seen);
    } else if (QFileInfo(homeDirectory).isAbsolute()) {
        appendAbsoluteRoot(QDir(homeDirectory).filePath(QStringLiteral(".local/share")),
                           &roots, &seen);
    }

    QString dataDirectories = environment.value(QStringLiteral("XDG_DATA_DIRS"));
    if (dataDirectories.isEmpty()) {
        dataDirectories = QStringLiteral("/usr/local/share:/usr/share");
    }
    for (const QString &directory :
         dataDirectories.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        appendAbsoluteRoot(directory, &roots, &seen);
    }
    return roots;
}

LauncherAppletComposition::LauncherAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    QStringList dataRoots,
    Services::SettingsClient::SettingsClient &settingsClient,
    const QDBusConnection &sessionBus,
    QStringList terminalCommand)
    : m_ownedSpawner(std::make_unique<Launcher::QProcessLaunchSpawner>())
    , m_ownedActivator(
          std::make_unique<Launcher::SessionBusActivator>(sessionBus))
{
    compose(catalog, policy, std::move(dataRoots), settingsClient,
            *m_ownedSpawner, *m_ownedActivator, std::move(terminalCommand));
}

LauncherAppletComposition::LauncherAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    QStringList dataRoots,
    Services::SettingsClient::SettingsClient &settingsClient,
    Launcher::LaunchSpawner &spawner,
    Launcher::LaunchActivator &activator,
    QStringList terminalCommand)
{
    compose(catalog, policy, std::move(dataRoots), settingsClient, spawner,
            activator, std::move(terminalCommand));
}

LauncherAppletComposition::~LauncherAppletComposition()
{
    if (m_scanner) {
        m_scanner->stop();
    }
}

void LauncherAppletComposition::compose(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    QStringList dataRoots,
    Services::SettingsClient::SettingsClient &settingsClient,
    Launcher::LaunchSpawner &spawner,
    Launcher::LaunchActivator &activator,
    QStringList terminalCommand)
{
    m_scanner = std::make_unique<Launcher::ApplicationScanner>(
        std::move(dataRoots));
    m_persistence =
        std::make_unique<Launcher::LauncherPersistenceController>(settingsClient);
    m_executor = std::make_unique<Launcher::LaunchExecutor>(
        *m_scanner, spawner, activator, std::move(terminalCommand));
    m_access = std::make_unique<Launcher::LauncherAppletController>(
        m_scanner.get(), m_persistence.get(), m_executor.get(),
        applicationsLaunchGranted(catalog, policy));
}

bool LauncherAppletComposition::start(QString *error)
{
    return m_scanner != nullptr && m_scanner->start(error);
}

Launcher::LauncherAppletController *
LauncherAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
