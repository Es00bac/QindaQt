// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Firewall::Client {
class FirewallClient;
class FirewallTransport;
}

namespace QindaQt::Shell::FirewallApplet {
class FirewallAppletController;
}

namespace QindaQt::Shell {

// Shell-private ownership boundary for the public Firewall1 client and the
// purpose-specific Firewall applet facade (ADR-0431). The audited
// manifest/registry/policy gates are evaluated once, independently of panel
// window reconstruction, exactly like NetworkAppletComposition. Unlike
// Network, a denied `security.read` grant constructs no transport or
// client at all (the Agent Usage applet's precedent): the controller then
// borrows nothing and presents permanently as Unavailable.
class FirewallAppletComposition final
{
public:
    FirewallAppletComposition(const Applets::ManifestCatalog &catalog,
                              const AppletHost::CapabilityPolicy &policy);
    ~FirewallAppletComposition();

    FirewallAppletComposition(const FirewallAppletComposition &) = delete;
    FirewallAppletComposition &operator=(const FirewallAppletComposition &) = delete;

    [[nodiscard]] FirewallApplet::FirewallAppletController *access() const noexcept;

private:
    // AGENT-CONTRACT: reverse member destruction is controller -> client ->
    // transport, matching NetworkAppletComposition. The destructor stops the
    // client before any owner is freed.
    std::unique_ptr<Firewall::Client::FirewallTransport> m_transport;
    std::unique_ptr<Firewall::Client::FirewallClient> m_client;
    std::unique_ptr<FirewallApplet::FirewallAppletController> m_access;
};

} // namespace QindaQt::Shell
