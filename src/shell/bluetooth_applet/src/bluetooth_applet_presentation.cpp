// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/bluetooth_applet/bluetooth_applet_presentation.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtCore/QStringList>

#include <algorithm>

namespace QindaQt::Shell::BluetoothApplet
{
namespace
{

QString fallbackDeviceLabel(const Bluetooth::DeviceClass deviceClass)
{
    switch (deviceClass) {
    case Bluetooth::DeviceClass::Computer:
        return QStringLiteral("Computer");
    case Bluetooth::DeviceClass::Phone:
        return QStringLiteral("Phone");
    case Bluetooth::DeviceClass::AudioVideo:
        return QStringLiteral("Audio or video device");
    case Bluetooth::DeviceClass::Headset:
        return QStringLiteral("Headset");
    case Bluetooth::DeviceClass::Headphones:
        return QStringLiteral("Headphones");
    case Bluetooth::DeviceClass::Keyboard:
        return QStringLiteral("Keyboard");
    case Bluetooth::DeviceClass::Mouse:
        return QStringLiteral("Mouse");
    case Bluetooth::DeviceClass::Tablet:
        return QStringLiteral("Tablet");
    case Bluetooth::DeviceClass::Printer:
        return QStringLiteral("Printer");
    case Bluetooth::DeviceClass::GameInput:
        return QStringLiteral("Game controller");
    case Bluetooth::DeviceClass::Wearable:
        return QStringLiteral("Wearable device");
    case Bluetooth::DeviceClass::Tag:
        return QStringLiteral("Tracking tag");
    case Bluetooth::DeviceClass::Unknown:
        return QStringLiteral("Bluetooth device");
    }
    return QStringLiteral("Bluetooth device");
}

const Bluetooth::Adapter *adapterFor(
    const QList<Bluetooth::Adapter> &adapters,
    const Bluetooth::Handle &handle)
{
    const auto it = std::ranges::find_if(
        adapters, [&handle](const Bluetooth::Adapter &candidate) {
            return candidate.handle == handle;
        });
    return it == adapters.cend() ? nullptr : &*it;
}

QString adapterDescription(const Bluetooth::Adapter &adapter,
                           const bool leaseOwned)
{
    QStringList states;
    states.append(adapter.powered ? QStringLiteral("powered on")
                                  : QStringLiteral("powered off"));
    states.append(adapter.discovering ? QStringLiteral("discovering devices")
                                      : QStringLiteral("not discovering"));
    if (leaseOwned) {
        states.append(QStringLiteral("discovery requested by this applet"));
    }
    return states.join(QStringLiteral(", "));
}

QString deviceDescription(const Bluetooth::Device &device)
{
    QStringList states;
    states.append(device.paired ? QStringLiteral("paired")
                                : QStringLiteral("not paired"));
    states.append(device.connected ? QStringLiteral("connected")
                                   : QStringLiteral("disconnected"));
    if (device.batteryKnown) {
        states.append(QStringLiteral("battery %1 percent").arg(device.batteryPercent));
    }
    if (device.rssiKnown) {
        states.append(QStringLiteral("signal %1 dBm").arg(device.rssi));
    }
    return states.join(QStringLiteral(", "));
}

} // namespace

QString adapterRowId(const Bluetooth::Handle &handle)
{
    return QStringLiteral("adapter-%1-%2").arg(handle.epoch).arg(handle.serial);
}

QString deviceRowId(const Bluetooth::Handle &handle)
{
    return QStringLiteral("device-%1-%2").arg(handle.epoch).arg(handle.serial);
}

BluetoothAppletModel projectBluetoothApplet(
    const Bluetooth::Snapshot &snapshot,
    const bool exactOwnerAvailable,
    const bool readGranted,
    const bool controlGranted,
    const std::optional<Bluetooth::Handle> discoveryLease)
{
    BluetoothAppletModel model;
    model.summaryLabel = QStringLiteral("Bluetooth");
    model.accessibleName = QStringLiteral("Bluetooth is unavailable");

    if (!readGranted) {
        model.diagnostic = QStringLiteral(
            "Bluetooth access was not granted to this applet.");
        model.accessibleDescription = model.diagnostic;
        return model;
    }
    if (!exactOwnerAvailable) {
        model.diagnostic = QStringLiteral("Bluetooth information is unavailable.");
        model.accessibleDescription = model.diagnostic;
        return model;
    }
    const Bluetooth::ValidationResult validation =
        Bluetooth::validateSnapshot(snapshot);
    if (!validation.accepted) {
        model.diagnostic = QStringLiteral("Bluetooth information was malformed.");
        model.accessibleDescription = model.diagnostic;
        return model;
    }

    if (snapshot.availability == Bluetooth::Availability::Starting) {
        model.phase = ServicePhase::Loading;
        model.summaryLabel = QStringLiteral("…");
        model.diagnostic = QStringLiteral("Bluetooth information is loading.");
        model.accessibleName = model.diagnostic;
        model.accessibleDescription = model.diagnostic;
        return model;
    }
    if (snapshot.availability != Bluetooth::Availability::Ready) {
        model.diagnostic = snapshot.reasonCode.isEmpty()
            ? QStringLiteral("Bluetooth information is unavailable.")
            : QStringLiteral("Bluetooth is unavailable: %1.")
                  .arg(snapshot.reasonCode);
        model.accessibleDescription = model.diagnostic;
        return model;
    }
    if (snapshot.adapters.size() > Bluetooth::kMaxAdapters
        || snapshot.devices.size() > Bluetooth::kMaxDevices) {
        model.diagnostic = QStringLiteral("Bluetooth inventory exceeded its bound.");
        model.accessibleDescription = model.diagnostic;
        return model;
    }

    model.phase = ServicePhase::Ready;
    model.epoch = snapshot.epoch;
    model.revision = snapshot.revision;
    QList<Bluetooth::Adapter> adapters = snapshot.adapters;
    std::ranges::sort(adapters, [](const Bluetooth::Adapter &left,
                                  const Bluetooth::Adapter &right) {
        return left.handle.serial < right.handle.serial;
    });

    const bool powerCapability = controlGranted
        && snapshot.capabilities.testFlag(Bluetooth::Capability::SetAdapterPower);
    const bool discoveryCapability = controlGranted
        && snapshot.capabilities.testFlag(Bluetooth::Capability::DiscoveryLease);
    model.adapters.reserve(adapters.size());
    for (qsizetype index = 0; index < adapters.size(); ++index) {
        const Bluetooth::Adapter &adapter = adapters.at(index);
        const bool ownsLease = discoveryLease.has_value()
            && *discoveryLease == adapter.handle;
        const QString label = adapter.name.trimmed().isEmpty()
            ? QStringLiteral("Bluetooth adapter %1").arg(index + 1)
            : adapter.name.trimmed();
        model.adapters.append({
            .id = adapterRowId(adapter.handle),
            .label = label,
            .powered = adapter.powered,
            .discovering = adapter.discovering,
            .canSetPowered = powerCapability,
            .canAcquireDiscovery = discoveryCapability && adapter.powered
                && !discoveryLease.has_value(),
            .canReleaseDiscovery = discoveryCapability && ownsLease,
            .accessibleName = QStringLiteral("Bluetooth adapter %1").arg(label),
            .accessibleDescription = adapterDescription(adapter, ownsLease),
        });
    }

    QList<Bluetooth::Device> devices = snapshot.devices;
    std::ranges::sort(devices, [](const Bluetooth::Device &left,
                                 const Bluetooth::Device &right) {
        return left.handle.serial < right.handle.serial;
    });
    const bool connectCapability = controlGranted
        && snapshot.capabilities.testFlag(Bluetooth::Capability::ConnectPaired);
    const bool disconnectCapability = controlGranted
        && snapshot.capabilities.testFlag(Bluetooth::Capability::DisconnectPaired);
    model.devices.reserve(devices.size());
    for (const Bluetooth::Device &device : devices) {
        const Bluetooth::Adapter *adapter = adapterFor(adapters, device.adapterHandle);
        if (adapter == nullptr) {
            model = {};
            model.summaryLabel = QStringLiteral("Bluetooth");
            model.diagnostic = QStringLiteral("Bluetooth inventory was inconsistent.");
            model.accessibleName = QStringLiteral("Bluetooth is unavailable");
            model.accessibleDescription = model.diagnostic;
            return model;
        }
        const QString classLabel = fallbackDeviceLabel(device.deviceClass);
        const QString label = device.name.trimmed().isEmpty()
            ? classLabel : device.name.trimmed();
        model.devices.append({
            .id = deviceRowId(device.handle),
            .adapterId = adapterRowId(device.adapterHandle),
            .label = label,
            .classLabel = classLabel,
            .paired = device.paired,
            .connected = device.connected,
            .batteryKnown = device.batteryKnown,
            .batteryPercent = device.batteryPercent,
            .signalKnown = device.rssiKnown,
            .signalDbm = device.rssi,
            .canConnect = connectCapability && device.paired
                && !device.connected && adapter->powered,
            .canDisconnect = disconnectCapability && device.connected,
            .accessibleName = QStringLiteral("Bluetooth device %1").arg(label),
            .accessibleDescription = deviceDescription(device),
        });
    }

    const qsizetype connected = std::ranges::count_if(
        devices, [](const Bluetooth::Device &device) { return device.connected; });
    model.summaryLabel = connected > 0
        ? QStringLiteral("Bluetooth %1").arg(connected)
        : QStringLiteral("Bluetooth");
    model.accessibleName = connected == 1
        ? QStringLiteral("Bluetooth, 1 device connected")
        : QStringLiteral("Bluetooth, %1 devices connected").arg(connected);
    model.accessibleDescription = model.adapters.isEmpty()
        ? QStringLiteral("No Bluetooth adapters are available")
        : QStringLiteral("%1 adapters and %2 devices")
              .arg(model.adapters.size()).arg(model.devices.size());
    return model;
}

} // namespace QindaQt::Shell::BluetoothApplet
