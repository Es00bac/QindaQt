// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluez_adapter_backend_p.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtCore/QMetaObject>

namespace QindaQt::Bluetooth
{
namespace
{

using Bluez::BluezAdapterState;
using Bluez::BluezDeviceState;
using Bluez::BluezInterfaces;
using Bluez::BluezManagedObjects;

struct MappedReply
{
    BackendOperationStatus status = BackendOperationStatus::Failed;
    QString reasonCode;
};

MappedReply mapErrorReply(const QString &errorName)
{
    if (!errorName.startsWith(QLatin1String("org.bluez.Error."))) {
        return {BackendOperationStatus::Uncertain,
                QStringLiteral("bluez-transport")};
    }
    if (errorName == QLatin1String("org.bluez.Error.NotReady")) {
        return {BackendOperationStatus::Rejected, QStringLiteral("adapter-off")};
    }
    if (errorName == QLatin1String("org.bluez.Error.DoesNotExist")
        || errorName == QLatin1String("org.bluez.Error.UnknownObject")) {
        return {BackendOperationStatus::Rejected, QStringLiteral("stale-handle")};
    }
    if (errorName == QLatin1String("org.bluez.Error.AlreadyConnected")) {
        return {BackendOperationStatus::Rejected,
                QStringLiteral("already-connected")};
    }
    if (errorName == QLatin1String("org.bluez.Error.NotConnected")) {
        return {BackendOperationStatus::Rejected,
                QStringLiteral("not-connected")};
    }
    return {BackendOperationStatus::Failed, QStringLiteral("bluez-error")};
}
} // namespace

BluezAdapterBackend::BluezAdapterBackend(const QDBusConnection &connection,
                                         QObject *parent)
    : AdapterBackend(parent)
    , d(std::make_unique<State>(connection, this))
{
    connect(&d->transport, &Bluez::BluezTransport::ownerChanged, this,
            [this](const QString &) { handleOwnerReplaced(); });
    connect(&d->transport, &Bluez::BluezTransport::managedObjectsReady, this,
            [this](const BluezManagedObjects &objects) {
                if (!d->running) {
                    return;
                }
                d->store.replaceFrom(objects);
                publish();
            });
    connect(&d->transport, &Bluez::BluezTransport::interfacesAdded, this,
            [this](const QString &path, const BluezInterfaces &interfaces) {
                if (!d->running) {
                    return;
                }
                d->store.upsertInterfaces(path, interfaces);
                publish();
            });
    connect(&d->transport, &Bluez::BluezTransport::interfacesRemoved, this,
            [this](const QString &path, const QStringList &interfaces) {
                if (!d->running) {
                    return;
                }
                retireObjects(path, interfaces);
                publish();
            });
    connect(&d->transport, &Bluez::BluezTransport::propertiesChanged, this,
            [this](const QString &path, const QString &interfaceName,
                   const QVariantMap &changed, const QStringList &invalidated) {
                if (!d->running) {
                    return;
                }
                applyProperties(path, interfaceName, changed, invalidated);
            });
    connect(&d->transport, &Bluez::BluezTransport::callFinished, this,
            [this](const quint64 callId, const bool succeeded,
                   const QString &errorName, const QString &errorMessage) {
                Q_UNUSED(errorMessage)
                if (!d->running) {
                    return;
                }
                handleCallFinished(callId, succeeded, errorName);
            });
}

BluezAdapterBackend::~BluezAdapterBackend()
{
    stop();
}

quint64 BluezAdapterBackend::start()
{
    if (d->running) {
        return d->generation;
    }
    ++d->generation;
    if (d->generation == 0) {
        ++d->generation;
    }
    d->running = true;
    // AGENT-GUARD: The port contract requires start() to return before this
    // run publishes. Every transport event below is an asynchronous reply or
    // signal, so the initial publication is inherently queued and fenced by
    // the generation captured in submit().
    d->transport.start();
    return d->generation;
}

void BluezAdapterBackend::stop()
{
    if (!d->running) {
        return;
    }
    d->running = false;
    ++d->generation;
    d->transport.stop();
    // AGENT-GUARD: Lease holds are state of one backend run bound to one
    // BlueZ owner. They must never cross a stop/start boundary or a BlueZ
    // owner transition, or discovery sessions no live caller requested would
    // be resurrected.
    d->store.clear();
    d->leases.clear();
    d->inflightAcquires.clear();
    d->outstanding.clear();
    d->queuedAcquires.clear();
}

void BluezAdapterBackend::submit(const quint64 operationId,
                                 const BackendRequest &request)
{
    const quint64 generation = d->generation;
    QMetaObject::invokeMethod(
        this,
        [this, operationId, request, generation] {
            // AGENT-GUARD: Real BlueZ operations complete asynchronously.
            // Applying on a queued invocation preserves the port contract
            // that a completion can never overtake the pending submission,
            // and the captured generation drops work superseded by stop().
            if (!d->running || generation != d->generation) {
                return;
            }
            applySubmit(operationId, request);
        },
        Qt::QueuedConnection);
}

void BluezAdapterBackend::releaseOwner(const QString &callerId)
{
    if (!d->running) {
        return;
    }
    for (auto it = d->leases.begin(); it != d->leases.end();) {
        if (it.key().callerId != callerId) {
            ++it;
            continue;
        }
        const QString address = it.key().adapterAddress;
        it = d->leases.erase(it);
        if (d->localLeaseTotal(address) == 0) {
            if (const BluezAdapterState *adapter =
                    d->store.adapterByAddress(address);
                adapter != nullptr && adapter->powered && adapter->discovering) {
                // Cleanup only: the lease is already released locally and the
                // platform truth reconciles through the property signal.
                (void)d->transport.stopDiscovery(adapter->path);
            }
        }
    }
    publish();
}

void BluezAdapterBackend::applySubmit(const quint64 operationId,
                                      const BackendRequest &request)
{
    switch (request.kind) {
    case OperationKind::SetAdapterPower:
        submitSetPower(operationId, request);
        return;
    case OperationKind::AcquireDiscovery:
        submitAcquire(operationId, request);
        return;
    case OperationKind::ReleaseDiscovery:
        submitRelease(operationId, request);
        return;
    case OperationKind::Connect:
        submitConnect(operationId, request);
        return;
    case OperationKind::Disconnect:
        submitDisconnect(operationId, request);
        return;
    default:
        finishOperation(operationId, BackendOperationStatus::Failed,
                        QStringLiteral("malformed-request"));
    }
}

void BluezAdapterBackend::submitSetPower(const quint64 operationId,
                                         const BackendRequest &request)
{
    const BluezAdapterState *adapter =
        d->store.adapterByAddress(request.adapterAddress);
    if (adapter == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    const quint64 callId =
        d->transport.setAdapterPowered(adapter->path, request.powered);
    if (callId == 0) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    d->outstanding.insert(callId,
                          {.operationId = operationId,
                           .kind = request.kind,
                           .callerId = request.callerId,
                           .adapterAddress = request.adapterAddress,
                           .deviceAddress = {},
                           .powered = request.powered});
}

void BluezAdapterBackend::submitAcquire(const quint64 operationId,
                                        const BackendRequest &request)
{
    const BluezAdapterState *adapter =
        d->store.adapterByAddress(request.adapterAddress);
    if (adapter == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    if (!adapter->powered) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("adapter-off"));
        return;
    }
    // AGENT-GUARD: The model validates projected bounds, but the backend owns
    // the live table: in-flight acquisitions that have not yet been confirmed
    // and externally-held synthetic rows count against the same caps here.
    const quint32 external = d->externallyDiscovering(request.adapterAddress) ? 1 : 0;
    const quint32 perAdapter = d->localLeaseTotal(request.adapterAddress)
        + d->inflightAcquires.value(request.adapterAddress, 0) + external;
    quint32 total = 0;
    for (auto it = d->inflightAcquires.cbegin(); it != d->inflightAcquires.cend();
         ++it) {
        total += it.value();
    }
    total += d->localLeaseTotal() + d->externalLeaseTotal();
    if (perAdapter >= kMaxDiscoveryLeasesPerAdapter || total >= kMaxDiscoveryLeasesTotal) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("too-many-leases"));
        return;
    }
    if (d->localLeaseTotal(request.adapterAddress) > 0) {
        // AGENT-GUARD: BlueZ discovery sessions are per-sender. All Bluetooth1
        // callers share this backend's one bus connection, so a second
        // reference joins the session already granted to this sender instead
        // of issuing a StartDiscovery BlueZ would answer AlreadyExists.
        ++d->leases[{request.callerId, request.adapterAddress}];
        publish();
        finishOperation(operationId, BackendOperationStatus::Succeeded,
                        QStringLiteral("lease-acquired"));
        return;
    }
    if (d->inflightAcquires.value(request.adapterAddress, 0) > 0) {
        ++d->inflightAcquires[request.adapterAddress];
        d->queuedAcquires[request.adapterAddress].append(
            {.operationId = operationId,
             .kind = request.kind,
             .callerId = request.callerId,
             .adapterAddress = request.adapterAddress,
             .deviceAddress = {},
             .powered = false});
        return;
    }
    d->inflightAcquires[request.adapterAddress] =
        d->inflightAcquires.value(request.adapterAddress, 0) + 1;
    const quint64 callId = d->transport.startDiscovery(adapter->path);
    if (callId == 0) {
        const quint32 inflight =
            d->inflightAcquires.value(request.adapterAddress, 0);
        if (inflight <= 1) {
            d->inflightAcquires.remove(request.adapterAddress);
        } else {
            d->inflightAcquires[request.adapterAddress] = inflight - 1;
        }
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    d->outstanding.insert(callId,
                          {.operationId = operationId,
                           .kind = request.kind,
                           .callerId = request.callerId,
                           .adapterAddress = request.adapterAddress,
                           .deviceAddress = {},
                           .powered = false});
}

void BluezAdapterBackend::submitRelease(const quint64 operationId,
                                        const BackendRequest &request)
{
    const BluezLeaseKey key{request.callerId, request.adapterAddress};
    const auto it = d->leases.find(key);
    if (it == d->leases.end() || it.value() == 0) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("no-lease"));
        return;
    }
    if (--it.value() == 0) {
        d->leases.erase(it);
        if (const BluezAdapterState *adapter =
                d->store.adapterByAddress(request.adapterAddress);
            adapter != nullptr && adapter->powered && adapter->discovering) {
            (void)d->transport.stopDiscovery(adapter->path);
        }
    }
    publish();
    finishOperation(operationId, BackendOperationStatus::Succeeded,
                    QStringLiteral("lease-released"));
}

void BluezAdapterBackend::submitConnect(const quint64 operationId,
                                        const BackendRequest &request)
{
    const BluezDeviceState *device = d->store.deviceByAddress(request.deviceAddress);
    if (device == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    if (!device->paired) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("not-paired"));
        return;
    }
    if (device->connected) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("already-connected"));
        return;
    }
    const BluezAdapterState *adapter = nullptr;
    if (const BluezAdapterState *parent = d->store.adapter(device->adapterPath);
        parent != nullptr) {
        adapter = parent;
    } else {
        adapter = d->store.adapterByAddress(request.adapterAddress);
    }
    if (adapter == nullptr || !adapter->powered) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("adapter-off"));
        return;
    }
    const quint64 callId = d->transport.connectDevice(device->path);
    if (callId == 0) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    insertDeviceCall(callId, operationId, request, OperationKind::Connect);
}

void BluezAdapterBackend::submitDisconnect(const quint64 operationId,
                                           const BackendRequest &request)
{
    const BluezDeviceState *device = d->store.deviceByAddress(request.deviceAddress);
    if (device == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    if (!device->connected) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("not-connected"));
        return;
    }
    const quint64 callId = d->transport.disconnectDevice(device->path);
    if (callId == 0) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    insertDeviceCall(callId, operationId, request, OperationKind::Disconnect);
}

void BluezAdapterBackend::insertDeviceCall(const quint64 callId,
                                           const quint64 operationId,
                                           const BackendRequest &request,
                                           const OperationKind kind)
{
    d->outstanding.insert(callId,
                          {.operationId = operationId,
                           .kind = kind,
                           .callerId = request.callerId,
                           .adapterAddress = request.adapterAddress,
                           .deviceAddress = request.deviceAddress,
                           .powered = false});
}

void BluezAdapterBackend::handleCallFinished(const quint64 callId,
                                             const bool succeeded,
                                             const QString &errorName)
{
    const auto it = d->outstanding.find(callId);
    if (it == d->outstanding.end()) {
        return;
    }
    const State::Outstanding call = it.value();
    d->outstanding.erase(it);

    switch (call.kind) {
    case OperationKind::SetAdapterPower: {
        if (!succeeded) {
            const MappedReply mapped = mapErrorReply(errorName);
            finishOperation(call.operationId, mapped.status, mapped.reasonCode);
            return;
        }
        if (const BluezAdapterState *adapter =
                d->store.adapterByAddress(call.adapterAddress);
            adapter != nullptr) {
            d->store.applyPowered(adapter->path, call.powered);
            if (!call.powered) {
                d->dropAdapterLeases(call.adapterAddress);
            }
        }
        publish();
        finishOperation(call.operationId, BackendOperationStatus::Succeeded,
                        QStringLiteral("adapter-power-set"));
        return;
    }
    case OperationKind::AcquireDiscovery: {
        const QList<State::Outstanding> queued =
            d->queuedAcquires.take(call.adapterAddress);
        d->inflightAcquires.remove(call.adapterAddress);
        const bool sessionActive =
            succeeded || errorName == QLatin1String("org.bluez.Error.AlreadyExists")
            || errorName == QLatin1String("org.bluez.Error.InProgress");
        if (sessionActive) {
            ++d->leases[{call.callerId, call.adapterAddress}];
            for (const State::Outstanding &waiter : queued) {
                ++d->leases[{waiter.callerId, waiter.adapterAddress}];
            }
            publish();
            finishOperation(call.operationId, BackendOperationStatus::Succeeded,
                            QStringLiteral("lease-acquired"));
            for (const State::Outstanding &waiter : queued) {
                finishOperation(waiter.operationId,
                                BackendOperationStatus::Succeeded,
                                QStringLiteral("lease-acquired"));
            }
            return;
        }
        const MappedReply mapped = mapErrorReply(errorName);
        finishOperation(call.operationId, mapped.status, mapped.reasonCode);
        for (const State::Outstanding &waiter : queued) {
            finishOperation(waiter.operationId, mapped.status,
                            mapped.reasonCode);
        }
        return;
    }
    case OperationKind::Connect: {
        if (succeeded
            || errorName == QLatin1String("org.bluez.Error.AlreadyConnected")) {
            if (const BluezDeviceState *device =
                    d->store.deviceByAddress(call.deviceAddress);
                device != nullptr) {
                d->store.applyConnected(device->path, true);
            }
            publish();
            finishOperation(call.operationId,
                            succeeded ? BackendOperationStatus::Succeeded
                                      : BackendOperationStatus::Rejected,
                            succeeded ? QStringLiteral("connected")
                                      : QStringLiteral("already-connected"));
            return;
        }
        const MappedReply mapped = mapErrorReply(errorName);
        finishOperation(call.operationId, mapped.status, mapped.reasonCode);
        return;
    }
    case OperationKind::Disconnect: {
        if (succeeded
            || errorName == QLatin1String("org.bluez.Error.NotConnected")) {
            if (const BluezDeviceState *device =
                    d->store.deviceByAddress(call.deviceAddress);
                device != nullptr) {
                d->store.applyConnected(device->path, false);
            }
            publish();
            finishOperation(call.operationId,
                            succeeded ? BackendOperationStatus::Succeeded
                                      : BackendOperationStatus::Rejected,
                            succeeded ? QStringLiteral("disconnected")
                                      : QStringLiteral("not-connected"));
            return;
        }
        const MappedReply mapped = mapErrorReply(errorName);
        finishOperation(call.operationId, mapped.status, mapped.reasonCode);
        return;
    }
    default:
        finishOperation(call.operationId, BackendOperationStatus::Failed,
                        QStringLiteral("backend-malformed"));
    }
}

} // namespace QindaQt::Bluetooth
