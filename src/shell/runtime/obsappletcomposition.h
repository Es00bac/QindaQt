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

namespace QindaQt::Obs {
class ObsClient;
class QtObsTransport;
class SecretServiceObsStore;
}

namespace QindaQt::Shell::ObsApplet {
class ObsAppletController;
}

namespace QindaQt::Shell {

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
// client for the whole session. The watch costs one stat of OBS's own config
// file per tick and touches the keyring only once that file says OBS is set
// up, so a desktop with no OBS never talks to the Secret Service at all.
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
    // Returns true once the client has been started; false while OBS is not
    // set up yet. Safe to call repeatedly — it starts the client at most once.
    [[nodiscard]] bool tryStart();

    bool m_readGranted = false;
    bool m_started = false;
    QTimer m_provisioningWatch;
    std::unique_ptr<Obs::QtObsTransport> m_transport;
    std::unique_ptr<Obs::ObsClient> m_client;
    std::unique_ptr<Obs::SecretServiceObsStore> m_secrets;
    std::unique_ptr<ObsApplet::ObsAppletController> m_access;
};

} // namespace QindaQt::Shell
