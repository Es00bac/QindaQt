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

namespace QindaQt::Services::SessionActions {
class SessionActionsClient;
}

class QAction;

namespace QindaQt::Shell::PowerApplet {
class PowerAppletController;
}

namespace QindaQt::Shell {

// Shell-private ownership boundary for the public Power1 and session-actions
// clients plus the purpose-specific applet facade. It evaluates the audited
// built-in host and capability gates once, then owns client start/stop
// independently of panel window reconstruction.
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
    // AGENT-CONTRACT: reverse destruction releases shortcut/controller before
    // both clients and the Power transport. The destructor stops both clients
    // before releasing any owner.
    std::unique_ptr<Power::QtPowerTransport> m_transport;
    std::unique_ptr<Power::PowerClient> m_client;
    std::unique_ptr<Services::SessionActions::SessionActionsClient>
        m_sessionActions;
    std::unique_ptr<PowerApplet::PowerAppletController> m_access;
    std::unique_ptr<QAction> m_lockAction;
};

} // namespace QindaQt::Shell
