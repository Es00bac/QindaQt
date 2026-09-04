// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusnotifierappletcomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h"
#include "qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h"

#include <QDebug>

#include <utility>

namespace QindaQt::Shell {
namespace {

using StatusNotifierCapabilityGrants = std::pair<bool, bool>;

// Fail-closed grant evaluation mirroring the Clipboard composition: a missing
// manifest, a non-builtin host, an unregistered entry point, or a policy
// evaluation failure withholds BOTH capabilities.
StatusNotifierCapabilityGrants statusNotifierGrants(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("status-notifier"));
    if (manifest == nullptr) {
        return {};
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return {};
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return {};
    }

    bool read = false;
    bool activate = false;
    for (const auto &decision : evaluated.decisions) {
        if (decision.capability == Applets::Capability::StatusItemRead) {
            read = decision.granted();
        } else if (decision.capability == Applets::Capability::StatusItemActivate) {
            activate = decision.granted();
        }
    }
    return {read, activate};
}

} // namespace

StatusNotifierAppletComposition::StatusNotifierAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    const QDBusConnection &sessionBus,
    QStringList iconThemeRoots)
{
    const auto [read, activate] = statusNotifierGrants(catalog, policy);
    m_watcher = std::make_unique<StatusNotifier::StatusNotifierWatcherService>(
        sessionBus);
    m_adapter =
        std::make_unique<StatusNotifierApplet::StatusNotifierMonitorAdapter>(
            sessionBus, std::move(iconThemeRoots));
    m_access =
        std::make_unique<StatusNotifierApplet::StatusNotifierAppletController>(
            m_adapter.get(), read, activate);
    if (read) {
        // The watcher service fails closed into NameOwnedElsewhere rather than
        // claiming a foreign-owned name; the adapter's monitor then reports
        // watcher-unavailable Degraded truth instead of a silently broken
        // tray, so a startup failure here is not fatal to the shell.
        QString watcherError;
        if (!m_watcher->start(&watcherError)) {
            qWarning().noquote()
                << "QindaQt shell could not start the StatusNotifier watcher:"
                << watcherError;
        }
        m_adapter->start();
    }
}

StatusNotifierAppletComposition::~StatusNotifierAppletComposition()
{
    // AGENT-GUARD: stop the adapter before the watcher service so the
    // monitor's sink detaches from the registry while every collaborator is
    // still alive; both stop() calls are idempotent.
    if (m_adapter) {
        m_adapter->stop();
    }
    if (m_watcher) {
        m_watcher->stop();
    }
}

StatusNotifierApplet::StatusNotifierAppletController *
StatusNotifierAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
