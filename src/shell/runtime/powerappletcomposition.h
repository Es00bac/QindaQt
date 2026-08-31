// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Power {
class PowerClient;
class QtPowerTransport;
}

namespace QindaQt::Shell::PowerApplet {
class PowerAppletController;
}

namespace QindaQt::Shell {

// Shell-private ownership boundary for the public Power1 client and the
// purpose-specific applet facade. It evaluates the audited built-in host and
// capability gates once, then owns client start/stop independently of panel
// window reconstruction.
class PowerAppletComposition final
{
public:
    PowerAppletComposition(const Applets::ManifestCatalog &catalog,
                           const AppletHost::CapabilityPolicy &policy);
    ~PowerAppletComposition();

    PowerAppletComposition(const PowerAppletComposition &) = delete;
    PowerAppletComposition &operator=(const PowerAppletComposition &) = delete;

    [[nodiscard]] PowerApplet::PowerAppletController *access() const noexcept;

private:
    // AGENT-CONTRACT: reverse member destruction is controller -> client ->
    // transport. The destructor stops the client before releasing any owner.
    std::unique_ptr<Power::QtPowerTransport> m_transport;
    std::unique_ptr<Power::PowerClient> m_client;
    std::unique_ptr<PowerApplet::PowerAppletController> m_access;
};

} // namespace QindaQt::Shell
