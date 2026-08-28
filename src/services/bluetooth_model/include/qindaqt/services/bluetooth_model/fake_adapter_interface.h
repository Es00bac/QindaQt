// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

namespace QindaQt::Bluetooth
{

// Provides bounded, deterministic Bluetooth state transitions without touching
// the real BlueZ or D-Bus. AGENT-GUARD: This interface is private to the model
// and must never be called from outside bluetooth_model or exposed through
// service boundaries.
class FakeAdapterInterface
{
public:
    FakeAdapterInterface();
    ~FakeAdapterInterface();

    struct DeviceState {
        Device device;
        qint64 createdAtRevision = 0;
    };

    void reset();

    // Adapter state management
    bool setAdapterPowered(const Handle &handle, bool powered);
    const QList<Adapter> &adapters() const { return m_adapters; }

    // Device discovery and pairing
    bool beginDiscovery(const Handle &adapterHandle);
    bool endDiscovery(const Handle &adapterHandle);
    bool addDiscoveredDevice(const Handle &adapterHandle, const QString &address,
                             const QString &name, qint16 rssi);

    // Device operations
    bool pairDevice(const Handle &deviceHandle);
    bool unpairDevice(const Handle &deviceHandle);
    bool connectDevice(const Handle &deviceHandle);
    bool disconnectDevice(const Handle &deviceHandle);
    bool trustDevice(const Handle &deviceHandle, bool trusted);

    const QList<Device> &devices() const { return m_devices; }

private:
    QList<Adapter> m_adapters;
    QList<Device> m_devices;

    Adapter *findAdapterMutable(const Handle &handle);
    Device *findDeviceMutable(const Handle &handle);
};

} // namespace QindaQt::Bluetooth
