// SPDX-License-Identifier: GPL-3.0-or-later

#include "networkappletcomposition.h"

#include "network_applet_controller.h"
#include "settingsroutelauncher.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/network_client/network_client.h"
#include "qindaqt/services/network_qt_transport/qt_network_transport.h"

#include <QtCore/QDebug>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell
{
namespace
{

struct NetworkAppletGrants {
    bool read = false;
    bool control = false;
};

NetworkAppletGrants networkAppletGrants(const Applets::ManifestCatalog &catalog,
                                        const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("network"));
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
    NetworkAppletGrants grants;
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::NetworkRead) {
            grants.read = true;
        } else if (decision.capability == Applets::Capability::NetworkControl) {
            grants.control = true;
        }
    }
    return grants;
}

} // namespace

NetworkAppletComposition::NetworkAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const NetworkAppletGrants grants = networkAppletGrants(catalog, policy);
    // AGENT-NOTE: Network1 lives on the session bus (Settings and the
    // airplane-mode key controller bind it the same way).
    m_transport = std::make_unique<Network::Client::QtNetworkTransport>(
        QDBusConnection::sessionBus());
    m_client = std::make_unique<Network::Client::NetworkClient>(*m_transport);
    m_access = std::make_unique<NetworkApplet::NetworkAppletController>(
        m_client.get(), grants.read, grants.control);
    if (grants.read) {
        QString error;
        if (!m_client->start(&error)) {
            qWarning().noquote() << "QindaQt shell Network1 client unavailable:" << error;
        }
    }
}

NetworkAppletComposition::~NetworkAppletComposition()
{
    m_client->stop();
}

NetworkApplet::NetworkAppletController *NetworkAppletComposition::access() const noexcept
{
    return m_access.get();
}

void NetworkAppletComposition::attachRoutes(SettingsRouteLauncher *launcher)
{
    if (m_access == nullptr || launcher == nullptr) {
        return;
    }
    m_access->setSettingsLaunch(
        [launcher] { return launcher->openRoute(QStringLiteral("network")); });
}

} // namespace QindaQt::Shell
