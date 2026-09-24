// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Network::Client {
class NetworkClient;
class QtNetworkTransport;
}

namespace QindaQt::Shell::NetworkApplet {
class NetworkAppletController;
}

namespace QindaQt::Shell {

class SettingsRouteLauncher;

// Shell-private ownership boundary for the public Network1 client and the
// purpose-specific Network applet facade (ADR-0258). The audited
// manifest/registry/policy gates are evaluated once, independently of panel
// window reconstruction. The shell gains no credential surface: secured
// first-use connections prompt in the separate network secret agent.
class NetworkAppletComposition final
{
public:
    NetworkAppletComposition(const Applets::ManifestCatalog &catalog,
                             const AppletHost::CapabilityPolicy &policy);
    ~NetworkAppletComposition();

    NetworkAppletComposition(const NetworkAppletComposition &) = delete;
    NetworkAppletComposition &operator=(const NetworkAppletComposition &) = delete;

    [[nodiscard]] NetworkApplet::NetworkAppletController *access() const noexcept;

    // Wires "Network Settings…" to the Settings `network` route. The launcher
    // is borrowed and must outlive this composition's controller.
    void attachRoutes(SettingsRouteLauncher *launcher);

private:
    // AGENT-CONTRACT: reverse member destruction is controller -> client ->
    // transport. The destructor stops the client before any owner is freed.
    std::unique_ptr<Network::Client::QtNetworkTransport> m_transport;
    std::unique_ptr<Network::Client::NetworkClient> m_client;
    std::unique_ptr<NetworkApplet::NetworkAppletController> m_access;
};

} // namespace QindaQt::Shell
