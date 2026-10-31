// SPDX-License-Identifier: GPL-3.0-or-later

#include "firewallappletcomposition.h"

#include "firewall_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/firewall_client/firewall_client.h"
#include "qindaqt/services/firewall_qt_transport/qt_firewall_transport.h"

#include <QtCore/QDebug>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell
{
namespace
{

bool securityReadGranted(const Applets::ManifestCatalog &catalog,
                         const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("firewall"));
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
        if (decision.capability == Applets::Capability::SecurityRead && decision.granted()) {
            return true;
        }
    }
    return false;
}

} // namespace

FirewallAppletComposition::FirewallAppletComposition(
    const Applets::ManifestCatalog &catalog, const AppletHost::CapabilityPolicy &policy)
{
    const bool granted = securityReadGranted(catalog, policy);
    // AGENT-GUARD: a denied grant constructs no transport and no client at
    // all (Agent Usage applet precedent) rather than an unstarted one, so
    // there is no live D-Bus watch to forget to stop.
    if (granted) {
        // AGENT-NOTE: org.qindaqt.Firewall1 lives on the session bus, owned
        // by QindaFirewall's background monitor (docs/monitor.md), the same
        // way Network1 is bound here.
        m_transport = std::make_unique<Firewall::Client::QtFirewallTransport>(
            QDBusConnection::sessionBus());
        m_client = std::make_unique<Firewall::Client::FirewallClient>(*m_transport);
    }
    m_access = std::make_unique<FirewallApplet::FirewallAppletController>(
        m_client.get(), granted);
    if (granted) {
        QString error;
        if (!m_client->start(&error)) {
            qWarning().noquote()
                << "QindaQt shell Firewall1 client unavailable:" << error;
        }
    }
}

FirewallAppletComposition::~FirewallAppletComposition()
{
    if (m_client) {
        m_client->stop();
    }
}

FirewallApplet::FirewallAppletController *FirewallAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
