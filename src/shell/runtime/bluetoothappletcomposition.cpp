// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetoothappletcomposition.h"

#include "bluetooth_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/bluetooth_client/bluetooth_client.h"
#include "qindaqt/services/bluetooth_client/qt_bluetooth_transport.h"

#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell
{
namespace
{

struct BluetoothAppletGrants {
    bool read = false;
    bool control = false;
};

BluetoothAppletGrants bluetoothAppletGrants(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("bluetooth"));
    if (manifest == nullptr) {
        return {};
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return {};
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return {};
    }
    BluetoothAppletGrants grants;
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::BluetoothRead) {
            grants.read = true;
        } else if (decision.capability == Applets::Capability::BluetoothControl) {
            grants.control = true;
        }
    }
    return grants;
}

} // namespace

BluetoothAppletComposition::BluetoothAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const BluetoothAppletGrants grants = bluetoothAppletGrants(catalog, policy);
    m_transport = std::make_unique<Bluetooth::QtBluetoothTransport>(
        QDBusConnection::sessionBus());
    m_client = std::make_unique<Bluetooth::BluetoothClient>(m_transport.get());
    m_access = std::make_unique<BluetoothApplet::BluetoothAppletController>(
        m_client.get(), grants.read, grants.control);
    if (grants.read) {
        m_client->start();
    }
}

BluetoothAppletComposition::~BluetoothAppletComposition()
{
    m_access->prepareForShutdown();
    m_client->stop();
}

BluetoothApplet::BluetoothAppletController *
BluetoothAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
