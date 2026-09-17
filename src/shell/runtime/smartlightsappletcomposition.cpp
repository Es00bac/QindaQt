// SPDX-License-Identifier: GPL-3.0-or-later

#include "smartlightsappletcomposition.h"

#include "smart_lights_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/smart_lights_store/configuration_store.h"
#include "qindaqt/services/wiz_client/wiz_client.h"
#include "qindaqt/services/wiz_udp_transport/wiz_udp_transport.h"

namespace QindaQt::Shell
{
namespace
{

struct SmartLightsAppletGrants {
    bool read = false;
    bool control = false;
};

SmartLightsAppletGrants smartLightsAppletGrants(
    const Applets::ManifestCatalog &catalog, const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("smart-lights"));
    if (manifest == nullptr) {
        return {};
    }
    const AppletHost::PackageIdentity package{manifest->id,
                                              AppletHost::PackageTrust::AuditedBuiltin};
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
    SmartLightsAppletGrants grants;
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::SmartLightRead) {
            grants.read = true;
        } else if (decision.capability == Applets::Capability::SmartLightControl) {
            grants.control = true;
        }
    }
    return grants;
}

} // namespace

SmartLightsAppletComposition::SmartLightsAppletComposition(
    const Applets::ManifestCatalog &catalog, const AppletHost::CapabilityPolicy &policy)
{
    const SmartLightsAppletGrants grants = smartLightsAppletGrants(catalog, policy);
    m_store = std::make_unique<SmartLights::ConfigurationStore>(
        SmartLights::ConfigurationStore::defaultPath());
    m_transport = std::make_unique<Wiz::WizUdpTransport>();
    m_clock = std::make_unique<Wiz::SystemWizClock>();
    m_client = std::make_unique<Wiz::WizClient>(m_transport.get(), m_clock.get());
    if (grants.read) {
        // Start before the controller so its stored endpoints are probed as
        // soon as they are adopted.
        m_client->start();
    }
    m_access = std::make_unique<SmartLightsApplet::SmartLightsAppletController>(
        m_client.get(), m_store.get(), grants.read, grants.control);
}

SmartLightsAppletComposition::~SmartLightsAppletComposition()
{
    m_client->stop();
}

SmartLightsApplet::SmartLightsAppletController *
SmartLightsAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
