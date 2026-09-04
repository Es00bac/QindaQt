// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_bluez_adapter/bluez_adapter_backend.h>

#include "bluez_object_store.h"
#include "bluez_pairing_agent.h"
#include "bluez_transport.h"

#include <QtCore/QHash>
#include <QtCore/QList>

namespace QindaQt::Bluetooth
{

struct BluezLeaseKey
{
    QString callerId;
    QString adapterAddress;

    friend bool operator==(const BluezLeaseKey &, const BluezLeaseKey &) = default;
};

inline size_t qHash(const BluezLeaseKey &key, const size_t seed) noexcept
{
    return qHashMulti(seed, key.callerId, key.adapterAddress);
}

// Private shared state for the lifecycle/publication and operation translation
// units. Keeping it private preserves the public pimpl boundary while letting
// each source retain one cohesive responsibility below the source-shape limit.
struct BluezAdapterBackend::State
{
    explicit State(const QDBusConnection &connection, QObject *transportParent,
                   const int promptTimeoutMs)
        : transport(connection, transportParent)
        , pairingAgent(connection,
                       [this](const QString &path) {
                           const auto *device = store.device(path);
                           return device == nullptr ? QString{} : device->address;
                       },
                       promptTimeoutMs, transportParent)
    {
    }

    struct Outstanding
    {
        quint64 operationId = 0;
        OperationKind kind = OperationKind::Connect;
        QString callerId;
        QString adapterAddress;
        QString deviceAddress;
        bool powered = false;
    };

    Bluez::BluezTransport transport;
    Bluez::BluezObjectStore store;
    Bluez::BluezPairingAgent pairingAgent;
    QHash<BluezLeaseKey, quint32> leases;
    QHash<QString, quint32> inflightAcquires;
    QHash<quint64, Outstanding> outstanding;
    QHash<QString, QList<Outstanding>> queuedAcquires;
    quint64 generation = 0;
    bool running = false;
    BackendPairingPrompt pairingPrompt;

    [[nodiscard]] quint32 localLeaseTotal(const QString &adapterAddress) const
    {
        quint32 total = 0;
        for (auto it = leases.cbegin(); it != leases.cend(); ++it) {
            if (it.key().adapterAddress == adapterAddress) {
                total += it.value();
            }
        }
        return total;
    }

    [[nodiscard]] quint32 localLeaseTotal() const
    {
        quint32 total = 0;
        for (auto it = leases.cbegin(); it != leases.cend(); ++it) {
            total += it.value();
        }
        return total;
    }

    [[nodiscard]] quint32 externalLeaseTotal() const
    {
        quint32 total = 0;
        const auto &adapters = store.adapters();
        for (auto it = adapters.cbegin(); it != adapters.cend(); ++it) {
            if (it.value().powered && it.value().discovering
                && localLeaseTotal(it.value().address) == 0) {
                ++total;
            }
        }
        return total;
    }

    [[nodiscard]] bool externallyDiscovering(
        const QString &adapterAddress) const
    {
        const Bluez::BluezAdapterState *adapter =
            store.adapterByAddress(adapterAddress);
        return adapter != nullptr && adapter->powered && adapter->discovering;
    }

    void dropAdapterLeases(const QString &adapterAddress)
    {
        for (auto it = leases.begin(); it != leases.end();) {
            if (it.key().adapterAddress == adapterAddress) {
                it = leases.erase(it);
            } else {
                ++it;
            }
        }
        inflightAcquires.remove(adapterAddress);
    }

    void dropAllLeases()
    {
        leases.clear();
        inflightAcquires.clear();
    }
};

} // namespace QindaQt::Bluetooth
