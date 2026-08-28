// SPDX-License-Identifier: GPL-3.0-or-later

#include "deterministic_adapter_backend.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtCore/QMetaObject>

#include <algorithm>

namespace QindaQt::Bluetooth
{
namespace
{

BackendAdapter *findBackendAdapter(BackendInventory &inventory, const QString &address)
{
    const auto it = std::find_if(inventory.adapters.begin(), inventory.adapters.end(),
                                 [&address](const BackendAdapter &adapter) {
                                     return adapter.address == address;
                                 });
    return it == inventory.adapters.end() ? nullptr : &*it;
}

BackendDevice *findBackendDevice(BackendInventory &inventory, const QString &address)
{
    const auto it = std::find_if(inventory.devices.begin(), inventory.devices.end(),
                                 [&address](const BackendDevice &device) {
                                     return device.address == address;
                                 });
    return it == inventory.devices.end() ? nullptr : &*it;
}

} // namespace

DeterministicAdapterBackend::DeterministicAdapterBackend(QObject *parent)
    : AdapterBackend(parent)
{
}

quint64 DeterministicAdapterBackend::start()
{
    ++m_generation;
    if (m_generation == 0) {
        ++m_generation;
    }
    m_running = true;
    // AGENT-GUARD: The port contract requires start() to return the
    // generation before that run may publish. The model installs the
    // generation fence only after this returns, so the initial publication
    // must be queued and generation-fenced, never emitted synchronously.
    const quint64 runGeneration = m_generation;
    QMetaObject::invokeMethod(
        this,
        [this, runGeneration] {
            if (m_running && runGeneration == m_generation) {
                publish();
            }
        },
        Qt::QueuedConnection);
    return m_generation;
}

void DeterministicAdapterBackend::stop()
{
    m_running = false;
    // AGENT-GUARD: Lease holds are caller-scoped state of one backend run.
    // They must never cross a stop/start boundary, or a restarted backend
    // would resurrect discovery sessions that no live caller requested.
    m_leases.clear();
    m_state.leases.clear();
    recomputeDiscovery();
}

void DeterministicAdapterBackend::publish()
{
    BackendInventory published = m_state;
    published.leases.clear();
    for (auto it = m_leases.cbegin(); it != m_leases.cend(); ++it) {
        if (it.value() != 0) {
            published.leases.push_back({it.key().callerId, it.key().adapterAddress,
                                        it.value()});
        }
    }
    m_state = published;
    Q_EMIT inventoryChanged(m_generation, m_state);
}

void DeterministicAdapterBackend::finish(const quint64 operationId,
                                         const BackendOperationStatus status,
                                         const QString &reasonCode)
{
    Q_EMIT operationFinished(m_generation, operationId,
                             {.status = status, .reasonCode = reasonCode, .diagnostic = {}});
}

quint32 DeterministicAdapterBackend::leaseTotal(const QString &adapterAddress) const
{
    quint32 total = 0;
    for (auto it = m_leases.cbegin(); it != m_leases.cend(); ++it) {
        if (it.key().adapterAddress == adapterAddress) {
            total += it.value();
        }
    }
    return total;
}

quint32 DeterministicAdapterBackend::leaseTotal() const
{
    quint32 total = 0;
    for (auto it = m_leases.cbegin(); it != m_leases.cend(); ++it) {
        total += it.value();
    }
    return total;
}

void DeterministicAdapterBackend::recomputeDiscovery()
{
    for (BackendAdapter &adapter : m_state.adapters) {
        adapter.discovering = adapter.powered && leaseTotal(adapter.address) != 0;
    }
}

void DeterministicAdapterBackend::setInventory(const BackendInventory &inventory)
{
    m_state = inventory;
    // AGENT-GUARD: A wholesale test-state replacement must not inherit stale
    // lease holds; leases belong to the simulation run, not the fixture data.
    m_leases.clear();
    recomputeDiscovery();
    publish();
}

void DeterministicAdapterBackend::submit(const quint64 operationId,
                                         const BackendRequest &request)
{
    ++m_submitCalls;
    // AGENT-GUARD: Real BluezQt operations complete asynchronously. Applying
    // on a queued invocation preserves that contract: the model returns a
    // pending submission before any completion signal can overtake it, which
    // the resident service object relies on to register its delayed reply.
    // The captured generation drops submissions that a stop/start boundary
    // superseded before they could run.
    const quint64 runGeneration = m_generation;
    QMetaObject::invokeMethod(
        this,
        [this, operationId, request, runGeneration] {
            if (runGeneration != m_generation) {
                return;
            }
            applySubmit(operationId, request);
        },
        Qt::QueuedConnection);
}

void DeterministicAdapterBackend::applySubmit(const quint64 operationId,
                                              const BackendRequest &request)
{
    if (!m_running) {
        finish(operationId, BackendOperationStatus::Uncertain,
               QStringLiteral("backend-stopped"));
        return;
    }

    switch (request.kind) {
    case OperationKind::SetAdapterPower: {
        BackendAdapter *adapter = findBackendAdapter(m_state, request.adapterAddress);
        if (adapter == nullptr) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("stale-handle"));
            return;
        }
        adapter->powered = request.powered;
        if (!adapter->powered) {
            // AGENT-GUARD: Powering off terminates the adapter's discovery
            // sessions and every connection on it, matching the BlueZ truth
            // the snapshot invariants state. That includes releasing the
            // adapter's leases: a later power-on must not resurrect
            // discovery that no surviving lease authorizes.
            for (auto it = m_leases.begin(); it != m_leases.end();) {
                if (it.key().adapterAddress == adapter->address) {
                    it = m_leases.erase(it);
                } else {
                    ++it;
                }
            }
            for (BackendDevice &device : m_state.devices) {
                if (device.adapterAddress == adapter->address) {
                    device.connected = false;
                }
            }
        }
        recomputeDiscovery();
        publish();
        finish(operationId, BackendOperationStatus::Succeeded,
               QStringLiteral("adapter-power-set"));
        return;
    }
    case OperationKind::AcquireDiscovery: {
        const BackendAdapter *adapter = findBackendAdapter(m_state, request.adapterAddress);
        if (adapter == nullptr) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("stale-handle"));
            return;
        }
        if (!adapter->powered) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("adapter-off"));
            return;
        }
        if (leaseTotal(request.adapterAddress) >= kMaxDiscoveryLeasesPerAdapter
            || leaseTotal() >= kMaxDiscoveryLeasesTotal) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("too-many-leases"));
            return;
        }
        ++m_leases[{request.callerId, request.adapterAddress}];
        recomputeDiscovery();
        publish();
        finish(operationId, BackendOperationStatus::Succeeded,
               QStringLiteral("lease-acquired"));
        return;
    }
    case OperationKind::ReleaseDiscovery: {
        const LeaseKey key{request.callerId, request.adapterAddress};
        const auto it = m_leases.find(key);
        if (it == m_leases.end() || it.value() == 0) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("no-lease"));
            return;
        }
        if (--it.value() == 0) {
            m_leases.erase(it);
        }
        recomputeDiscovery();
        publish();
        finish(operationId, BackendOperationStatus::Succeeded,
               QStringLiteral("lease-released"));
        return;
    }
    case OperationKind::Connect: {
        BackendDevice *device = findBackendDevice(m_state, request.deviceAddress);
        if (device == nullptr) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("stale-handle"));
            return;
        }
        if (!device->paired) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("not-paired"));
            return;
        }
        if (device->connected) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("already-connected"));
            return;
        }
        const BackendAdapter *adapter = findBackendAdapter(m_state, device->adapterAddress);
        if (adapter == nullptr || !adapter->powered) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("adapter-off"));
            return;
        }
        device->connected = true;
        publish();
        finish(operationId, BackendOperationStatus::Succeeded,
               QStringLiteral("connected"));
        return;
    }
    case OperationKind::Disconnect: {
        BackendDevice *device = findBackendDevice(m_state, request.deviceAddress);
        if (device == nullptr) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("stale-handle"));
            return;
        }
        if (!device->connected) {
            finish(operationId, BackendOperationStatus::Rejected,
                   QStringLiteral("not-connected"));
            return;
        }
        device->connected = false;
        publish();
        finish(operationId, BackendOperationStatus::Succeeded,
               QStringLiteral("disconnected"));
        return;
    }
    default:
        finish(operationId, BackendOperationStatus::Failed,
               QStringLiteral("malformed-request"));
        return;
    }
}

void DeterministicAdapterBackend::releaseOwner(const QString &callerId)
{
    ++m_releaseOwnerCalls;
    bool changed = false;
    for (auto it = m_leases.begin(); it != m_leases.end();) {
        if (it.key().callerId == callerId) {
            it = m_leases.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) {
        recomputeDiscovery();
        publish();
    }
}

} // namespace QindaQt::Bluetooth
