// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_model/fake_adapter_interface.h>

namespace QindaQt::Bluetooth
{

FakeAdapterInterface::FakeAdapterInterface() = default;

FakeAdapterInterface::~FakeAdapterInterface() = default;

void FakeAdapterInterface::reset()
{
    m_adapters.clear();
    m_devices.clear();
}

bool FakeAdapterInterface::setAdapterPowered(const Handle &handle, bool powered)
{
    auto adapter = findAdapterMutable(handle);
    if (!adapter) {
        return false;
    }
    adapter->state = powered ? AdapterState::On : AdapterState::Off;
    return true;
}

bool FakeAdapterInterface::beginDiscovery(const Handle &adapterHandle)
{
    auto adapter = findAdapterMutable(adapterHandle);
    if (!adapter) {
        return false;
    }
    adapter->discoveringActive = true;
    return true;
}

bool FakeAdapterInterface::endDiscovery(const Handle &adapterHandle)
{
    auto adapter = findAdapterMutable(adapterHandle);
    if (!adapter) {
        return false;
    }
    adapter->discoveringActive = false;
    return true;
}

bool FakeAdapterInterface::addDiscoveredDevice(const Handle &adapterHandle,
                                               const QString &address, const QString &name,
                                               qint16 rssi)
{
    if (!findAdapterMutable(adapterHandle)) {
        return false;
    }
    // Fake: just store device state, don't emit discovery signals
    return true;
}

bool FakeAdapterInterface::pairDevice(const Handle &deviceHandle)
{
    auto device = findDeviceMutable(deviceHandle);
    if (!device) {
        return false;
    }
    device->paired = true;
    return true;
}

bool FakeAdapterInterface::unpairDevice(const Handle &deviceHandle)
{
    auto device = findDeviceMutable(deviceHandle);
    if (!device) {
        return false;
    }
    device->paired = false;
    device->state = DeviceState::Disconnected;
    device->trusted = false;
    return true;
}

bool FakeAdapterInterface::connectDevice(const Handle &deviceHandle)
{
    auto device = findDeviceMutable(deviceHandle);
    if (!device || !device->paired) {
        return false;
    }
    device->state = DeviceState::Connected;
    return true;
}

bool FakeAdapterInterface::disconnectDevice(const Handle &deviceHandle)
{
    auto device = findDeviceMutable(deviceHandle);
    if (!device) {
        return false;
    }
    device->state = DeviceState::Disconnected;
    return true;
}

bool FakeAdapterInterface::trustDevice(const Handle &deviceHandle, bool trusted)
{
    auto device = findDeviceMutable(deviceHandle);
    if (!device) {
        return false;
    }
    device->trusted = trusted;
    return true;
}

Adapter *FakeAdapterInterface::findAdapterMutable(const Handle &handle)
{
    for (auto &adapter : m_adapters) {
        if (adapter.handle == handle) {
            return &adapter;
        }
    }
    return nullptr;
}

Device *FakeAdapterInterface::findDeviceMutable(const Handle &handle)
{
    for (auto &device : m_devices) {
        if (device.handle == handle) {
            return &device;
        }
    }
    return nullptr;
}

} // namespace QindaQt::Bluetooth
