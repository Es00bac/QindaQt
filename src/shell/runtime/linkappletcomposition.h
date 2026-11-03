// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Link {
class LinkClient;
class LinkTransport;
}

namespace QindaQt::Shell::LinkApplet {
class LinkAppletController;
}

namespace QindaQt::Shell::Launcher {
class LauncherAppletController;
}

namespace QindaQt::Shell {

// Shell-private ownership boundary for the public Link1 client and the
// QindaLink applet facade (ADR-0437, docs/wiki/shell/link-applet.md). The
// audited manifest/registry/policy gates are evaluated once. A denied
// `phone.read` grant constructs no transport or client at all (Firewall and
// Agent Usage precedent): the controller then presents permanently as
// Unavailable. `phone.control` is evaluated separately.
class LinkAppletComposition final
{
public:
    LinkAppletComposition(const Applets::ManifestCatalog &catalog,
                          const AppletHost::CapabilityPolicy &policy);
    ~LinkAppletComposition();

    LinkAppletComposition(const LinkAppletComposition &) = delete;
    LinkAppletComposition &operator=(const LinkAppletComposition &) = delete;

    [[nodiscard]] LinkApplet::LinkAppletController *access() const noexcept;

    // Wires "Open QindaLink" through the shell launcher. The launcher is
    // borrowed and must outlive the controller; a null launcher leaves the
    // action hidden.
    void attachLauncher(Launcher::LauncherAppletController *launcher);

private:
    // AGENT-CONTRACT: reverse member destruction is controller -> client ->
    // transport. The destructor stops the client before any owner is freed.
    std::unique_ptr<Link::LinkTransport> m_transport;
    std::unique_ptr<Link::LinkClient> m_client;
    std::unique_ptr<LinkApplet::LinkAppletController> m_access;
};

} // namespace QindaQt::Shell
