// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Bluetooth {
class BluetoothClient;
class QtBluetoothTransport;
}

namespace QindaQt::Shell::BluetoothApplet {
class BluetoothAppletController;
}

namespace QindaQt::Shell {

// Shell-private ownership boundary for the public Bluetooth1 client and the
// purpose-specific built-in facade. The audited manifest/registry/policy gates
// are evaluated once, independently of panel-window reconstruction.
class BluetoothAppletComposition final
{
public:
    BluetoothAppletComposition(const Applets::ManifestCatalog &catalog,
                               const AppletHost::CapabilityPolicy &policy);
    ~BluetoothAppletComposition();

    BluetoothAppletComposition(const BluetoothAppletComposition &) = delete;
    BluetoothAppletComposition &operator=(const BluetoothAppletComposition &) = delete;

    [[nodiscard]] BluetoothApplet::BluetoothAppletController *access() const noexcept;

private:
    // AGENT-CONTRACT: reverse member destruction is controller -> client ->
    // transport. The destructor gives the controller one final lease-release
    // dispatch opportunity before stopping the client.
    std::unique_ptr<Bluetooth::QtBluetoothTransport> m_transport;
    std::unique_ptr<Bluetooth::BluetoothClient> m_client;
    std::unique_ptr<BluetoothApplet::BluetoothAppletController> m_access;
};

} // namespace QindaQt::Shell
