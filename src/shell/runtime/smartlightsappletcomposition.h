// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::SmartLights {
class ConfigurationStore;
}

namespace QindaQt::Wiz {
class WizClient;
class WizUdpTransport;
class SystemWizClock;
}

namespace QindaQt::Shell::SmartLightsApplet {
class SmartLightsAppletController;
}

namespace QindaQt::Shell {

// Shell-private ownership boundary for the Wiz LAN client, the stored
// configuration, and the purpose-specific built-in facade. The audited
// manifest/registry/policy gates are evaluated once, independently of
// panel-window reconstruction.
//
// AGENT-CONTRACT: the client is only started when the read capability is
// granted. Without it nothing is sent to the network at all: no discovery
// broadcast, no polling, and no subscription datagram.
class SmartLightsAppletComposition final
{
public:
    SmartLightsAppletComposition(const Applets::ManifestCatalog &catalog,
                                 const AppletHost::CapabilityPolicy &policy);
    ~SmartLightsAppletComposition();

    SmartLightsAppletComposition(const SmartLightsAppletComposition &) = delete;
    SmartLightsAppletComposition &operator=(const SmartLightsAppletComposition &) = delete;

    [[nodiscard]] SmartLightsApplet::SmartLightsAppletController *access() const noexcept;

private:
    // AGENT-CONTRACT: reverse member destruction is controller -> client ->
    // clock/transport/store. The controller may still persist configuration
    // while it is torn down, so the store outlives it.
    std::unique_ptr<SmartLights::ConfigurationStore> m_store;
    std::unique_ptr<Wiz::WizUdpTransport> m_transport;
    std::unique_ptr<Wiz::SystemWizClock> m_clock;
    std::unique_ptr<Wiz::WizClient> m_client;
    std::unique_ptr<SmartLightsApplet::SmartLightsAppletController> m_access;
};

} // namespace QindaQt::Shell
