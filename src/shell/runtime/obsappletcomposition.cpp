// SPDX-License-Identifier: GPL-3.0-or-later

#include "obsappletcomposition.h"

#include "obs_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/obs_client/obs_client.h"
#include "qindaqt/services/obs_client/obs_provisioning.h"
#include "qindaqt/services/obs_client/obs_secret_store.h"
#include "qindaqt/services/obs_client/qt_obs_transport.h"

#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell {
namespace {

struct ObsAppletGrants {
    bool read = false;
    bool control = false;
};

ObsAppletGrants obsAppletGrants(const Applets::ManifestCatalog &catalog,
                                const AppletHost::CapabilityPolicy &policy) {
    const auto *manifest = catalog.findById(QStringLiteral("obs"));
    if (manifest == nullptr) {
        return {};
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin ||
        !registry.contains(manifest->entryPoint.value)) {
        return {};
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return {};
    }
    ObsAppletGrants grants;
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::StreamingRead) {
            grants.read = true;
        } else if (decision.capability == Applets::Capability::StreamingControl) {
            grants.control = true;
        }
    }
    return grants;
}

} // namespace

ObsAppletComposition::ObsAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy) {
    const ObsAppletGrants grants = obsAppletGrants(catalog, policy);
    m_transport = std::make_unique<Obs::QtObsTransport>();
    m_client = std::make_unique<Obs::ObsClient>(*m_transport);
    m_secrets = std::make_unique<Obs::SecretServiceObsStore>(
        QDBusConnection::sessionBus());
    if (grants.read) {
        QString keyringError;
        const auto password = m_secrets->password(&keyringError);
        // AGENT-GUARD: no password, no connection. Connecting without one
        // would make OBS's refusal look like a wrong password the user
        // chose, and would retry against it forever. The Settings Streaming
        // route is where a password is created.
        if (password.has_value()) {
            bool found = false;
            const Obs::WebSocketSettings configured =
                Obs::readWebSocketSettings(Obs::defaultObsConfigRoot(), &found);
            const int port = found && configured.serverPort > 0
                                 ? configured.serverPort
                                 : 4455;
            m_client->start(QStringLiteral("ws://127.0.0.1:%1").arg(port),
                            *password);
        }
    }
    // The controller is built either way: without a grant it presents the
    // same honest "OBS is not running" state rather than vanishing from the
    // panel the user configured.
    m_access = std::make_unique<ObsApplet::ObsAppletController>(
        grants.control ? m_client.get() : nullptr);
}

ObsAppletComposition::~ObsAppletComposition() = default;

ObsApplet::ObsAppletController *ObsAppletComposition::access() const noexcept {
    return m_access.get();
}

} // namespace QindaQt::Shell
