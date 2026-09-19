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

// Slow on purpose: this is a "has the user set OBS up yet" watch, not a
// connection retry. Once the client is started, ObsClient owns its own
// reconnect backoff and this timer stops for good.
constexpr int kProvisioningWatchMilliseconds = 5000;

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
    m_readGranted = grants.read;
    m_transport = std::make_unique<Obs::QtObsTransport>();
    m_client = std::make_unique<Obs::ObsClient>(*m_transport);
    m_secrets = std::make_unique<Obs::SecretServiceObsStore>(
        QDBusConnection::sessionBus());
    if (m_readGranted && !tryStart()) {
        // OBS is not set up yet. Watch for it rather than giving up until the
        // next login: the user's very next action may be to set it up from
        // Settings -> Streaming, and the applet should come to life then.
        m_provisioningWatch.setInterval(kProvisioningWatchMilliseconds);
        QObject::connect(&m_provisioningWatch, &QTimer::timeout,
                         &m_provisioningWatch, [this] {
                             if (tryStart()) {
                                 m_provisioningWatch.stop();
                             }
                         });
        m_provisioningWatch.start();
    }
    // Read access keeps the live indicator even when control is denied.
    m_access = std::make_unique<ObsApplet::ObsAppletController>(
        grants.read ? m_client.get() : nullptr, nullptr, grants.control);
}

bool ObsAppletComposition::tryStart() {
    if (!m_readGranted || m_started) {
        return m_started;
    }
    // AGENT-GUARD: OBS's own config file is the cheap gate, and it is checked
    // FIRST. Until it says obs-websocket is set up there is no reason to ask
    // the Secret Service anything, so a desktop without OBS never generates
    // keyring traffic on this path however long it runs.
    bool found = false;
    const Obs::WebSocketSettings configured =
        Obs::readWebSocketSettings(Obs::defaultObsConfigRoot(), &found);
    const auto url = Obs::obsControlUrl(configured, found);
    if (!url.has_value()) {
        return false;
    }
    QString keyringError;
    const auto password = m_secrets->password(&keyringError);
    // AGENT-GUARD: no password, no connection. Connecting without one would
    // make OBS's refusal look like a wrong password the user chose, and would
    // retry against it forever. The Settings Streaming route is where a
    // password is created.
    if (!password.has_value()) {
        return false;
    }
    m_client->start(*url, *password);
    m_started = true;
    return true;
}

ObsAppletComposition::~ObsAppletComposition() = default;

ObsApplet::ObsAppletController *ObsAppletComposition::access() const noexcept {
    return m_access.get();
}

} // namespace QindaQt::Shell
