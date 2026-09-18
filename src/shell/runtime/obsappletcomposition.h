// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

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
    std::unique_ptr<Obs::QtObsTransport> m_transport;
    std::unique_ptr<Obs::ObsClient> m_client;
    std::unique_ptr<Obs::SecretServiceObsStore> m_secrets;
    std::unique_ptr<ObsApplet::ObsAppletController> m_access;
};

} // namespace QindaQt::Shell
