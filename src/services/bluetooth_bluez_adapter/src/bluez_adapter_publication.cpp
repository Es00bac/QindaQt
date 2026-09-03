// SPDX-License-Identifier: LGPL-3.0-or-later

// Publication and owner-retirement half of BluezAdapterBackend. Operation
// admission/translation stays in bluez_adapter_backend.cpp; both halves share
// only the module-private state in bluez_adapter_backend_p.h.

#include "bluez_adapter_backend_p.h"

#include <QtCore/QSet>

#include <utility>

namespace QindaQt::Bluetooth
{
namespace
{

using Bluez::BluezAdapterState;
using Bluez::BluezInterfaces;

constexpr QLatin1StringView kAdapterInterface{"org.bluez.Adapter1"};
constexpr QLatin1StringView kDeviceInterface{"org.bluez.Device1"};
// AGENT-NOTE: BlueZ discovery sessions are reference-counted per client, so
// another client can keep an adapter discovering while Bluetooth1 holds no
// lease. The model requires the lease table to agree with each adapter's
// discovering flag, so platform truth is represented as one synthetic row.
constexpr QLatin1StringView kExternalLeaseCaller{":bluez-external-session"};

} // namespace

void BluezAdapterBackend::handleOwnerReplaced()
{
    if (!d->running) {
        return;
    }
    // AGENT-GUARD: Any org.bluez owner transition retires all truth bound to
    // the previous owner before a fresh enumeration may publish.
    const auto outstanding = std::exchange(d->outstanding, {});
    for (auto it = outstanding.cbegin(); it != outstanding.cend(); ++it) {
        finishOperation(it.value().operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("authority-replaced"));
    }
    const auto queuedAcquires = std::exchange(d->queuedAcquires, {});
    for (auto adapterIt = queuedAcquires.cbegin();
         adapterIt != queuedAcquires.cend(); ++adapterIt) {
        for (const State::Outstanding &call : adapterIt.value()) {
            finishOperation(call.operationId,
                            BackendOperationStatus::Uncertain,
                            QStringLiteral("authority-replaced"));
        }
    }
    d->dropAllLeases();
    d->pairingPrompt = {};
    d->store.clear();
    publish();
}

void BluezAdapterBackend::applyProperties(const QString &path,
                                          const QString &interfaceName,
                                          const QVariantMap &changed,
                                          const QStringList &invalidated)
{
    if (interfaceName == QString(kAdapterInterface)) {
        if (d->store.adapter(path) == nullptr) {
            return;
        }
        const bool discoveringReported =
            changed.contains(QStringLiteral("Discovering"));
        BluezInterfaces patch;
        patch.insert(interfaceName, changed);
        d->store.upsertInterfaces(path, patch);
        const BluezAdapterState *after = d->store.adapter(path);
        if (after == nullptr) {
            return;
        }
        if (!after->powered || (discoveringReported && !after->discovering)) {
            d->dropAdapterLeases(after->address);
        }
    } else if (interfaceName == QString(kDeviceInterface)) {
        if (d->store.device(path) == nullptr) {
            return;
        }
        BluezInterfaces patch;
        patch.insert(interfaceName, changed);
        d->store.upsertInterfaces(path, patch);
    }
    if (!invalidated.isEmpty()) {
        // Invalidated properties are unknown, not defaulted: enumerate again
        // instead of fabricating defaults into the accepted snapshot.
        d->transport.requestManagedObjects();
    }
    publish();
}

void BluezAdapterBackend::retireObjects(const QString &path,
                                        const QStringList &interfaces)
{
    QString adapterAddress;
    if (interfaces.contains(QString(kAdapterInterface))) {
        if (const BluezAdapterState *adapter = d->store.adapter(path);
            adapter != nullptr) {
            adapterAddress = adapter->address;
        }
    }
    d->store.removeInterfaces(path, interfaces);
    if (!adapterAddress.isEmpty()) {
        d->dropAdapterLeases(adapterAddress);
    }
}

void BluezAdapterBackend::publish()
{
    for (auto it = d->leases.begin(); it != d->leases.end();) {
        const BluezAdapterState *adapter =
            d->store.adapterByAddress(it.key().adapterAddress);
        if (adapter == nullptr || !adapter->powered) {
            it = d->leases.erase(it);
        } else {
            ++it;
        }
    }
    BackendInventory inventory;
    inventory.adapters = d->store.projectAdapters();
    d->pairingAgent.setAdapterAvailable(!inventory.adapters.isEmpty());
    QSet<QString> adapterAddresses;
    for (const BackendAdapter &adapter : inventory.adapters) {
        adapterAddresses.insert(adapter.address);
    }
    inventory.devices = d->store.projectDevices(adapterAddresses);
    inventory.pairingPrompt = d->pairingPrompt;
    for (BackendAdapter &adapter : inventory.adapters) {
        const quint32 local = d->localLeaseTotal(adapter.address);
        const bool external = adapter.powered && local == 0
            && d->externallyDiscovering(adapter.address);
        // AGENT-GUARD: The model requires discovering == (powered && leases).
        adapter.discovering = adapter.powered && (local > 0 || external);
        if (external) {
            inventory.leases.push_back(
                {QString(kExternalLeaseCaller), adapter.address, 1});
        }
    }
    for (auto it = d->leases.cbegin(); it != d->leases.cend(); ++it) {
        if (it.value() > 0) {
            inventory.leases.push_back(
                {it.key().callerId, it.key().adapterAddress, it.value()});
        }
    }
    Q_EMIT inventoryChanged(d->generation, inventory);
}

void BluezAdapterBackend::finishOperation(const quint64 operationId,
                                          const BackendOperationStatus status,
                                          const QString &reasonCode)
{
    Q_EMIT operationFinished(
        d->generation, operationId,
        {.status = status, .reasonCode = reasonCode, .diagnostic = {}});
}

} // namespace QindaQt::Bluetooth
