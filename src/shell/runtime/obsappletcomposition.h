// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QTimer>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
class QtSettingsTransport;
}
namespace QindaQt::Services::StreamingPreferences {
class Settings1StreamingPreferences;
}

namespace QindaQt::Obs {
class ObsClient;
class QtObsTransport;
class SecretServiceObsStore;
}

namespace QindaQt::Shell::ObsApplet {
class ObsAppletController;
}

namespace QindaQt::Shell {
class ObsAppletConnection;

// Shell-private ownership boundary for the obs-websocket client and the
// built-in OBS facade. The audited manifest/registry/policy gates are
// evaluated once, independently of panel-window reconstruction.
//
// AGENT-CONTRACT: the client is only started when the read capability is
// granted, and only after the obs-websocket password has been read from the
// keyring. Without the grant nothing is sent anywhere: no socket, no
// connection attempt, no reconnect timer.
//
// AGENT-CONTRACT: the start condition is re-checked, not read once. The normal
// first run is that the shell is already up when the user sets OBS up from
// Settings -> Streaming, so at construction there is no config and no secret.
// Reading them once meant the applet stayed dead until the next login, which
// is exactly what the 2026-09-18 end-to-end check saw: obs-websocket logged no
// client for the whole session. The watch remains active after startup for
// confirmed preference/active-port changes; it touches the keyring only when
// a confirmed baseline enables connection and OBS config matches its port.
//
// AGENT-GUARD: the password reaches the client and nothing else. It is never
// held by the controller, never published into QML, and never logged.
class ObsAppletComposition final {
public:
    ObsAppletComposition(const Applets::ManifestCatalog &catalog,
                         const AppletHost::CapabilityPolicy &policy);
    ~ObsAppletComposition();

    ObsAppletComposition(const ObsAppletComposition &) = delete;
    ObsAppletComposition &operator=(const ObsAppletComposition &) = delete;

    [[nodiscard]] ObsApplet::ObsAppletController *access() const noexcept;

private:
    bool m_readGranted = false;
    QTimer m_provisioningWatch;
    std::unique_ptr<Obs::QtObsTransport> m_transport;
    std::unique_ptr<Obs::ObsClient> m_client;
    std::unique_ptr<Obs::SecretServiceObsStore> m_secrets;
    std::unique_ptr<Services::SettingsClient::QtSettingsTransport> m_settingsTransport;
    std::unique_ptr<Services::SettingsClient::SettingsClient> m_settingsClient;
    std::unique_ptr<Services::StreamingPreferences::Settings1StreamingPreferences> m_preferences;
    std::unique_ptr<ObsAppletConnection> m_connection;
    std::unique_ptr<ObsApplet::ObsAppletController> m_access;
};

} // namespace QindaQt::Shell
