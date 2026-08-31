// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerappletcomposition.h"

#include "power_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/power_client/power_client.h"
#include "qindaqt/services/power_client/qt_power_transport.h"

#include <QDBusConnection>

namespace QindaQt::Shell {
namespace {

struct PowerAppletGrants {
    bool read = false;
    bool control = false;
};

PowerAppletGrants powerAppletGrants(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("power"));
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
    PowerAppletGrants grants;
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::PowerRead) {
            grants.read = true;
        } else if (decision.capability == Applets::Capability::PowerControl) {
            grants.control = true;
        }
    }
    return grants;
}

} // namespace

PowerAppletComposition::PowerAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const PowerAppletGrants grants = powerAppletGrants(catalog, policy);
    m_transport = std::make_unique<Power::QtPowerTransport>(
        QDBusConnection::sessionBus());
    m_client = std::make_unique<Power::PowerClient>(m_transport.get());
    m_access = std::make_unique<PowerApplet::PowerAppletController>(
        m_client.get(), grants.read, grants.control);
    if (grants.read) {
        m_client->start();
    }
}

PowerAppletComposition::~PowerAppletComposition()
{
    m_client->stop();
}

PowerApplet::PowerAppletController *PowerAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
