// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/bluetooth_bluez_adapter/bluez_adapter_backend.h>

#include "bluez_object_store.h"
#include "bluez_transport.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QSet>

#include <utility>

namespace QindaQt::Bluetooth
{
namespace
{

using Bluez::BluezAdapterState;
using Bluez::BluezDeviceState;
using Bluez::BluezInterfaces;
using Bluez::BluezManagedObjects;
using Bluez::BluezObjectStore;
using Bluez::BluezTransport;

constexpr QLatin1StringView kAdapterInterface{"org.bluez.Adapter1"};
constexpr QLatin1StringView kDeviceInterface{"org.bluez.Device1"};
// AGENT-NOTE: BlueZ discovery sessions are reference-counted per client, so
// another client can keep an adapter discovering while Bluetooth1 holds no
// lease. The model requires the lease table to agree with each adapter's
// discovering flag, so that platform truth is represented as one synthetic
// external-session row. It never reaches the public wire: the Bluetooth1
// snapshot carries no lease list.
constexpr QLatin1StringView kExternalLeaseCaller{":bluez-external-session"};

struct LeaseKey
{
    QString callerId;
    QString adapterAddress;

    friend bool operator==(const LeaseKey &, const LeaseKey &) = default;
};

size_t qHash(const LeaseKey &key, const size_t seed) noexcept
{
    return qHashMulti(seed, key.callerId, key.adapterAddress);
}

bool isBluezBusinessError(const QString &errorName)
{
    return errorName.startsWith(QLatin1String("org.bluez.Error."));
}

struct MappedReply
{
    BackendOperationStatus status = BackendOperationStatus::Failed;
    QString reasonCode;
};

// BlueZ error names are platform truth; Bluetooth1 reason codes are the
// stable programmatic surface. Unmapped transport failures (timeouts, bus
// loss, owner replacement) are uncertain, never failures: the platform
// outcome is genuinely unknown.
MappedReply mapErrorReply(const QString &errorName)
{
    if (!isBluezBusinessError(errorName)) {
        return {BackendOperationStatus::Uncertain, QStringLiteral("bluez-transport")};
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
        return {BackendOperationStatus::Rejected, QStringLiteral("not-connected")};
    }
    return {BackendOperationStatus::Failed, QStringLiteral("bluez-error")};
}

} // namespace

struct BluezAdapterBackend::State
{
    explicit State(const QDBusConnection &connection, QObject *transportParent)
        : transport(connection, transportParent)
    {
    }

    BluezTransport transport;
    BluezObjectStore store;
    QHash<LeaseKey, quint32> leases;
    QHash<QString, quint32> inflightAcquires;
    struct Outstanding
    {
        quint64 operationId = 0;
        OperationKind kind = OperationKind::Connect;
        QString callerId;
        QString adapterAddress;
        QString deviceAddress;
        bool powered = false;
    };
    QHash<quint64, Outstanding> outstanding;
    quint64 generation = 0;
    bool running = false;

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

    [[nodiscard]] bool externallyDiscovering(const QString &adapterAddress) const
    {
        const BluezAdapterState *adapter = store.adapterByAddress(adapterAddress);
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
    const LeaseKey key{request.callerId, request.adapterAddress};
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
        const quint32 inflight = d->inflightAcquires.value(call.adapterAddress, 0);
        if (inflight <= 1) {
            d->inflightAcquires.remove(call.adapterAddress);
        } else {
            d->inflightAcquires[call.adapterAddress] = inflight - 1;
        }
        const bool sessionActive =
            succeeded || errorName == QLatin1String("org.bluez.Error.AlreadyExists")
            || errorName == QLatin1String("org.bluez.Error.InProgress");
        if (sessionActive) {
            ++d->leases[{call.callerId, call.adapterAddress}];
            publish();
            finishOperation(call.operationId, BackendOperationStatus::Succeeded,
                            QStringLiteral("lease-acquired"));
            return;
        }
        const MappedReply mapped = mapErrorReply(errorName);
        finishOperation(call.operationId, mapped.status, mapped.reasonCode);
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

void BluezAdapterBackend::handleOwnerReplaced()
{
    if (!d->running) {
        return;
    }
    // AGENT-GUARD: Any org.bluez owner transition (loss, replacement, or
    // return) retires every piece of truth bound to the previous owner:
    // outstanding operations complete uncertain, leases and inflight
    // acquisitions die with the daemon's sessions, and the store empties so
    // the model republishes the truthful Unavailable/no-adapter snapshot
    // until the fresh owner's inventory arrives.
    const auto outstanding = std::exchange(d->outstanding, {});
    for (auto it = outstanding.cbegin(); it != outstanding.cend(); ++it) {
        finishOperation(it.value().operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("authority-replaced"));
    }
    d->dropAllLeases();
    d->store.clear();
    publish();
}

void BluezAdapterBackend::applyProperties(const QString &path,
                                          const QString &interfaceName,
                                          const QVariantMap &changed,
                                          const QStringList &invalidated)
{
    if (interfaceName == QString(kAdapterInterface)) {
        const BluezAdapterState *before = d->store.adapter(path);
        if (before == nullptr) {
            return;
        }
        const bool discoveringReported = changed.contains(QStringLiteral("Discovering"));
        BluezInterfaces patch;
        patch.insert(interfaceName, changed);
        d->store.upsertInterfaces(path, patch);
        const BluezAdapterState *after = d->store.adapter(path);
        if (after == nullptr) {
            return;
        }
        // AGENT-GUARD: A powered-off adapter has no sessions and a
        // Discovering=false transition means the platform killed the session;
        // local lease references die with it so the table never claims a
        // session the platform no longer runs.
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
        // Invalidated properties are unknown, not defaulted: re-read the
        // authoritative managed-object snapshot instead of guessing.
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
    // Leases can only reference live, powered adapters; reconciling first
    // keeps the table the model validates consistent even when a snapshot
    // replace dropped adapters without InterfacesRemoved.
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
    QSet<QString> adapterAddresses;
    for (const BackendAdapter &adapter : inventory.adapters) {
        adapterAddresses.insert(adapter.address);
    }
    inventory.devices = d->store.projectDevices(adapterAddresses);
    for (BackendAdapter &adapter : inventory.adapters) {
        const quint32 local = d->localLeaseTotal(adapter.address);
        const bool external =
            adapter.powered && local == 0 && d->externallyDiscovering(adapter.address);
        // AGENT-GUARD: The model requires discovering == (powered && leases).
        // Discovery authority is the lease table plus BlueZ's own sessions.
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
    Q_EMIT operationFinished(d->generation, operationId,
                             {.status = status, .reasonCode = reasonCode, .diagnostic = {}});
}

} // namespace QindaQt::Bluetooth
