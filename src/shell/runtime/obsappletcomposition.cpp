// SPDX-License-Identifier: GPL-3.0-or-later

#include "obsappletcomposition.h"
#include "obsappletconnection.h"

#include "obs_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/obs_client/obs_client.h"
#include "qindaqt/services/obs_client/obs_provisioning.h"
#include "qindaqt/services/obs_client/obs_secret_store.h"
#include "qindaqt/services/obs_client/qt_obs_transport.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/streaming_preferences/settings1_streaming_preferences.h"

#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell {
namespace {

// Slow on purpose: this is a "has the user set OBS up yet" watch, not a
// connection retry. ObsClient owns socket retry; this timer continues
// watching setup and selected-port changes throughout the applet lifetime.
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
    if (m_readGranted) {
        m_settingsTransport = std::make_unique<Services::SettingsClient::QtSettingsTransport>(
            QDBusConnection::sessionBus());
        m_settingsClient = std::make_unique<Services::SettingsClient::SettingsClient>(
            *m_settingsTransport, Services::StreamingPreferences::Settings1StreamingPreferences::scopedKeys());
        m_preferences = std::make_unique<Services::StreamingPreferences::Settings1StreamingPreferences>(
            *m_settingsClient);
        m_connection = std::make_unique<ObsAppletConnection>(
            *m_client, *m_secrets, *m_preferences, []() -> std::optional<int> {
                bool found = false;
                const auto configured = Obs::readWebSocketSettings(
                    Obs::defaultObsConfigRoot(), &found);
                return Obs::obsControlUrl(configured, found).has_value()
                           ? std::optional<int>(configured.serverPort) : std::nullopt;
            });
        QObject::connect(m_preferences.get(),
                         &Services::StreamingPreferences::Settings1StreamingPreferences::preferencesChanged,
                         &m_provisioningWatch, [this] { (void)m_connection->reconcile(); });
        QString settingsError;
        (void)m_settingsClient->start(&settingsError);
        // Keep watching after connection: confirmed preference, OBS setup,
        // and active-port changes can occur while the applet remains mounted.
        m_provisioningWatch.setInterval(kProvisioningWatchMilliseconds);
        QObject::connect(&m_provisioningWatch, &QTimer::timeout,
                         &m_provisioningWatch, [this] { (void)m_connection->reconcile(); });
        m_provisioningWatch.start();
    }
    // Read access keeps the live indicator even when control is denied.
    m_access = std::make_unique<ObsApplet::ObsAppletController>(
        grants.read ? m_client.get() : nullptr, nullptr, grants.control);
}


ObsAppletComposition::~ObsAppletComposition() = default;

ObsApplet::ObsAppletController *ObsAppletComposition::access() const noexcept {
    return m_access.get();
}

} // namespace QindaQt::Shell
