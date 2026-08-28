// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/adapter_backend.h>

namespace QindaQt::Tests
{

class FakeAdapterBackend final : public Bluetooth::AdapterBackend
{
public:
    using AdapterBackend::AdapterBackend;

    quint64 start() override
    {
        ++startCalls;
        ++generation;
        if (generation == 0) {
            ++generation;
        }
        running = true;
        return generation;
    }
    void stop() override
    {
        ++stopCalls;
        running = false;
    }
    void submit(const quint64 operationId,
                const Bluetooth::BackendRequest &request) override
    {
        operations.push_back({operationId, request});
    }
    void releaseOwner(const QString &callerId) override
    {
        releasedOwners.push_back(callerId);
    }

    void publish(const Bluetooth::BackendInventory &inventory)
    {
        publishForGeneration(generation, inventory);
    }
    void publishForGeneration(const quint64 runGeneration,
                              const Bluetooth::BackendInventory &inventory)
    {
        Q_EMIT inventoryChanged(runGeneration, inventory);
    }
    void finish(const quint64 operationId,
                const Bluetooth::BackendOperationOutcome &outcome)
    {
        finishForGeneration(generation, operationId, outcome);
    }
    void finishForGeneration(const quint64 runGeneration, const quint64 operationId,
                             const Bluetooth::BackendOperationOutcome &outcome)
    {
        Q_EMIT operationFinished(runGeneration, operationId, outcome);
    }

    struct RecordedOperation {
        quint64 operationId = 0;
        Bluetooth::BackendRequest request;
    };

    QList<RecordedOperation> operations;
    QList<QString> releasedOwners;
    int startCalls = 0;
    int stopCalls = 0;
    quint64 generation = 0;
    bool running = false;
};

inline Bluetooth::BackendInventory bluetoothInventory()
{
    Bluetooth::BackendInventory inventory;
    inventory.adapters = {{.address = QStringLiteral("AA:BB:CC:00:11:22"),
                           .name = QStringLiteral("Internal adapter"),
                           .powered = true,
                           .discovering = false}};
    inventory.devices = {{.adapterAddress = QStringLiteral("AA:BB:CC:00:11:22"),
                          .address = QStringLiteral("AA:BB:CC:33:44:55"),
                          .name = QStringLiteral("Keyboard"),
                          .deviceClass = Bluetooth::DeviceClass::Keyboard,
                          .paired = true,
                          .connected = false,
                          .rssiKnown = true,
                          .rssi = -52}};
    return inventory;
}

} // namespace QindaQt::Tests
