// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtCore/QDateTime>
#include <QtCore/QRandomGenerator>

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::Bluetooth
{
namespace
{

constexpr quint64 kFnvOffsetBasis = 14695981039346656037ULL;
constexpr quint64 kFnvPrime = 1099511628211ULL;

// Stable privacy-safe serials derive from the canonical address, never from
// list position or a transient platform object ID. Adapters and devices use
// distinct prefixes so one epoch's serial space cannot collide across kinds.
quint64 stableSerial(const char prefix, const QString &address)
{
    quint64 hash = kFnvOffsetBasis;
    const QByteArray bytes = address.toUtf8();
    hash ^= static_cast<quint64>(prefix);
    hash *= kFnvPrime;
    for (const char byte : bytes) {
        hash ^= static_cast<quint64>(static_cast<unsigned char>(byte));
        hash *= kFnvPrime;
    }
    return hash == 0 ? 1 : hash;
}

bool validBackendReasonCode(const QString &reasonCode)
{
    return isStructuredReasonCode(reasonCode);
}

bool safeCallerId(const QString &callerId)
{
    if (callerId.isEmpty() || !isBoundedText(callerId, kMaxCallerIdUtf8Bytes)) {
        return false;
    }
    for (const QChar character : callerId) {
        if (character.category() == QChar::Other_Control) {
            return false;
        }
    }
    return true;
}

quint32 adapterLeaseTotal(const BackendInventory &inventory, const QString &adapterAddress)
{
    quint32 total = 0;
    for (const BackendLease &lease : inventory.leases) {
        if (lease.adapterAddress == adapterAddress) {
            total += lease.refcount;
        }
    }
    return total;
}

quint32 totalLeases(const BackendInventory &inventory)
{
    quint32 total = 0;
    for (const BackendLease &lease : inventory.leases) {
        total += lease.refcount;
    }
    return total;
}

} // namespace

BluetoothModel::BluetoothModel(AdapterBackend *backend, const quint64 epochSeed,
                               QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
    Q_ASSERT(m_backend != nullptr);
    qRegisterMetaType<BackendInventory>();
    qRegisterMetaType<BackendOperationOutcome>();
    m_snapshot.schemaVersion = kSchemaVersion;
    m_snapshot.epoch = epochSeed != 0 ? epochSeed : advanceEpoch();
    m_snapshot.revision = 1;
    m_snapshot.availability = Availability::Starting;
    m_snapshot.reasonCode = QStringLiteral("starting");

    connect(m_backend, &AdapterBackend::inventoryChanged, this,
            &BluetoothModel::acceptInventory);
    connect(m_backend, &AdapterBackend::operationFinished, this,
            &BluetoothModel::acceptBackendResult);
}

const Snapshot &BluetoothModel::snapshot() const noexcept
{
    return m_snapshot;
}

void BluetoothModel::start()
{
    if (m_running) {
        return;
    }
    const quint64 generation = m_backend->start();
    if (generation == 0) {
        return;
    }
    if (m_hasInventory) {
        // AGENT-GUARD: A reused model must never issue handles that an earlier
        // backend run already issued. Advancing the epoch before the new run
        // can publish invalidates every outstanding handle and drops the
        // superseded run's lease/inventory projection.
        Snapshot restarting;
        restarting.schemaVersion = kSchemaVersion;
        restarting.epoch = advanceEpoch();
        restarting.revision = 1;
        restarting.availability = Availability::Starting;
        restarting.reasonCode = QStringLiteral("backend-restarting");
        m_inventory = BackendInventory{};
        m_snapshot = restarting;
        m_hasInventory = false;
        Q_EMIT snapshotChanged(m_snapshot);
        Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
    }
    m_backendGeneration = generation;
    m_running = true;
}

void BluetoothModel::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_backendGeneration = 0;
    m_backend->stop();
    makePendingUncertain(QStringLiteral("model-stopped"));
}

quint64 BluetoothModel::advanceEpoch()
{
    quint64 candidate = (static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) << 8U)
        | static_cast<quint64>(QRandomGenerator::global()->generate() & 0xFFU);
    if (candidate == 0) {
        candidate = 1;
    }
    if (m_snapshot.epoch != 0 && candidate <= m_snapshot.epoch) {
        candidate = m_snapshot.epoch + 1;
    }
    return candidate;
}

OperationResult BluetoothModel::immediate(const OperationRequest &request,
                                          const OperationStatus status,
                                          const QString &reasonCode) const
{
    return {.kind = request.kind,
            .status = status,
            .initiatingEpoch = m_snapshot.epoch,
            .initiatingRevision = m_snapshot.revision,
            .observedEpoch = m_snapshot.epoch,
            .observedRevision = m_snapshot.revision,
            .reasonCode = reasonCode,
            .diagnostic = {},
            .wireValid = true};
}

const Adapter *BluetoothModel::findAdapter(const quint64 serial) const
{
    const auto it = std::find_if(m_snapshot.adapters.cbegin(), m_snapshot.adapters.cend(),
                                 [serial](const Adapter &adapter) {
                                     return adapter.handle.serial == serial;
                                 });
    return it == m_snapshot.adapters.cend() ? nullptr : &*it;
}

const Device *BluetoothModel::findDevice(const quint64 serial) const
{
    const auto it = std::find_if(m_snapshot.devices.cbegin(), m_snapshot.devices.cend(),
                                 [serial](const Device &device) {
                                     return device.handle.serial == serial;
                                 });
    return it == m_snapshot.devices.cend() ? nullptr : &*it;
}

QString BluetoothModel::validateRequest(const OperationRequest &request,
                                        const QString &callerId) const
{
    if (!m_running || m_snapshot.availability != Availability::Ready) {
        return QStringLiteral("unavailable");
    }
    if (!safeCallerId(callerId)) {
        return QStringLiteral("malformed-caller");
    }
    if (!request.target.isValid() || request.target.epoch != m_snapshot.epoch) {
        return QStringLiteral("stale-handle");
    }

    switch (request.kind) {
    case OperationKind::SetAdapterPower: {
        if (!m_snapshot.capabilities.testFlag(Capability::SetAdapterPower)) {
            return QStringLiteral("unsupported");
        }
        if (findAdapter(request.target.serial) == nullptr) {
            return QStringLiteral("stale-handle");
        }
        return {};
    }
    case OperationKind::AcquireDiscovery: {
        if (!m_snapshot.capabilities.testFlag(Capability::DiscoveryLease)) {
            return QStringLiteral("unsupported");
        }
        const Adapter *adapter = findAdapter(request.target.serial);
        if (adapter == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!adapter->powered) {
            return QStringLiteral("adapter-off");
        }
        const QString address = adapter->address;
        if (adapterLeaseTotal(m_inventory, address) >= kMaxDiscoveryLeasesPerAdapter
            || totalLeases(m_inventory) >= kMaxDiscoveryLeasesTotal) {
            return QStringLiteral("too-many-leases");
        }
        return {};
    }
    case OperationKind::ReleaseDiscovery: {
        if (!m_snapshot.capabilities.testFlag(Capability::DiscoveryLease)) {
            return QStringLiteral("unsupported");
        }
        if (findAdapter(request.target.serial) == nullptr) {
            return QStringLiteral("stale-handle");
        }
        return {};
    }
    case OperationKind::Connect: {
        if (!m_snapshot.capabilities.testFlag(Capability::ConnectPaired)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(request.target.serial);
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!device->paired) {
            return QStringLiteral("not-paired");
        }
        const Adapter *adapter = findAdapter(device->adapterHandle.serial);
        if (adapter == nullptr || !adapter->powered) {
            return QStringLiteral("adapter-off");
        }
        return {};
    }
    case OperationKind::Disconnect: {
        if (!m_snapshot.capabilities.testFlag(Capability::DisconnectPaired)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(request.target.serial);
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!device->connected) {
            return QStringLiteral("not-connected");
        }
        return {};
    }
    default:
        return QStringLiteral("malformed-request");
    }
}

OperationSubmission BluetoothModel::submit(const OperationRequest &request,
                                           const QString &callerId)
{
    const QString rejection = validateRequest(request, callerId);
    if (!rejection.isEmpty()) {
        const OperationStatus status = rejection == QStringLiteral("unsupported")
            ? OperationStatus::Unsupported
            : OperationStatus::Rejected;
        return {.pending = false,
                .operationId = 0,
                .immediateResult = immediate(request, status, rejection)};
    }
    if (m_pending.size() >= kMaxInFlightOperations || m_nextOperationId == 0
        || m_nextOperationId == std::numeric_limits<quint64>::max()) {
        return {.pending = false,
                .operationId = 0,
                .immediateResult = immediate(request, OperationStatus::Busy,
                                             QStringLiteral("too-many-operations"))};
    }

    // AGENT-GUARD: Every pending operation stores the epoch/revision that
    // initiated it. Completion rebuilds the result from this stored lineage so
    // an intervening snapshot publication can never erase the initiator.
    const quint64 operationId = m_nextOperationId++;
    m_pending.insert(operationId,
                     {.kind = request.kind,
                      .epoch = m_snapshot.epoch,
                      .revision = m_snapshot.revision});

    BackendRequest backendRequest;
    backendRequest.kind = request.kind;
    backendRequest.powered = request.powered;
    backendRequest.callerId = callerId;
    if (const Adapter *adapter = findAdapter(request.target.serial); adapter != nullptr) {
        backendRequest.adapterAddress = adapter->address;
    }
    if (const Device *device = findDevice(request.target.serial); device != nullptr) {
        backendRequest.deviceAddress = device->address;
        const Adapter *adapter = findAdapter(device->adapterHandle.serial);
        backendRequest.adapterAddress = adapter != nullptr ? adapter->address : QString{};
    }
    m_backend->submit(operationId, backendRequest);
    return {.pending = true, .operationId = operationId, .immediateResult = {}};
}

void BluetoothModel::ownerVanished(const QString &callerId)
{
    if (!safeCallerId(callerId)) {
        return;
    }
    m_backend->releaseOwner(callerId);
}

void BluetoothModel::makePendingUncertain(const QString &reasonCode)
{
    const auto pending = std::exchange(m_pending, {});
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        const PendingOperation &operation = it.value();
        Q_EMIT operationCompleted(
            it.key(),
            {.kind = operation.kind,
             .status = OperationStatus::Uncertain,
             .initiatingEpoch = operation.epoch,
             .initiatingRevision = operation.revision,
             .observedEpoch = m_snapshot.epoch,
             .observedRevision = m_snapshot.revision,
             .reasonCode = reasonCode,
             .diagnostic = {},
             .wireValid = true});
    }
}

Snapshot BluetoothModel::projectInventory(const BackendInventory &inventory) const
{
    Snapshot snapshot;
    snapshot.schemaVersion = kSchemaVersion;
    snapshot.epoch = m_snapshot.epoch;
    snapshot.revision = m_snapshot.revision;
    if (inventory.adapters.isEmpty()) {
        snapshot.availability = Availability::Unavailable;
        snapshot.reasonCode = QStringLiteral("no-adapter");
        return snapshot;
    }
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capabilities::fromInt(
        static_cast<quint32>(Capability::SetAdapterPower)
        | static_cast<quint32>(Capability::DiscoveryLease)
        | static_cast<quint32>(Capability::ConnectPaired)
        | static_cast<quint32>(Capability::DisconnectPaired));
    snapshot.reasonCode = QStringLiteral("ready");
    for (const BackendAdapter &backendAdapter : inventory.adapters) {
        Adapter adapter;
        adapter.handle = {.epoch = snapshot.epoch,
                          .serial = stableSerial('a', backendAdapter.address)};
        adapter.address = backendAdapter.address;
        adapter.name = backendAdapter.name;
        adapter.powered = backendAdapter.powered;
        adapter.discovering = backendAdapter.discovering;
        snapshot.adapters.push_back(adapter);
    }
    for (const BackendDevice &backendDevice : inventory.devices) {
        Device device;
        device.handle = {.epoch = snapshot.epoch,
                         .serial = stableSerial('d', backendDevice.address)};
        device.adapterHandle = {.epoch = snapshot.epoch,
                                .serial = stableSerial('a', backendDevice.adapterAddress)};
        device.address = backendDevice.address;
        device.name = backendDevice.name;
        device.deviceClass = backendDevice.deviceClass;
        device.paired = backendDevice.paired;
        device.connected = backendDevice.connected;
        device.rssiKnown = backendDevice.rssiKnown;
        device.rssi = backendDevice.rssi;
        snapshot.devices.push_back(device);
    }
    std::sort(snapshot.adapters.begin(), snapshot.adapters.end(),
              [](const Adapter &left, const Adapter &right) {
                  return left.handle.serial < right.handle.serial;
              });
    std::sort(snapshot.devices.begin(), snapshot.devices.end(),
              [](const Device &left, const Device &right) {
                  return left.handle.serial < right.handle.serial;
              });
    return snapshot;
}

bool BluetoothModel::leaseBoundsRespected(const BackendInventory &inventory) const
{
    QHash<QString, quint32> perAdapter;
    quint32 total = 0;
    for (const BackendLease &lease : inventory.leases) {
        if (!safeCallerId(lease.callerId) || lease.refcount == 0
            || !isCanonicalAddress(lease.adapterAddress)) {
            return false;
        }
        perAdapter[lease.adapterAddress] += lease.refcount;
        total += lease.refcount;
        if (total > kMaxDiscoveryLeasesTotal) {
            return false;
        }
    }
    for (auto it = perAdapter.cbegin(); it != perAdapter.cend(); ++it) {
        if (it.value() > kMaxDiscoveryLeasesPerAdapter) {
            return false;
        }
    }
    return true;
}

void BluetoothModel::publishBackendMalformed()
{
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    Snapshot degraded;
    degraded.schemaVersion = kSchemaVersion;
    degraded.epoch = m_snapshot.epoch;
    degraded.revision = m_snapshot.revision + 1;
    degraded.availability = Availability::Degraded;
    degraded.reasonCode = QStringLiteral("backend-malformed");
    m_inventory = BackendInventory{};
    makePendingUncertain(QStringLiteral("backend-malformed"));
    m_snapshot = degraded;
    m_hasInventory = true;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void BluetoothModel::acceptInventory(const quint64 generation,
                                     const BackendInventory &inventory)
{
    // AGENT-GUARD: stop() and every later start supersede already queued
    // backend values. Check the run before validation so stale malformed data
    // cannot mutate even the fail-closed projection.
    if (!m_running || generation == 0 || generation != m_backendGeneration) {
        return;
    }

    const bool inventorySane = inventory.adapters.size() <= kMaxAdapters
        && inventory.devices.size() <= kMaxDevices && leaseBoundsRespected(inventory)
        && std::all_of(inventory.adapters.cbegin(), inventory.adapters.cend(),
                       [](const BackendAdapter &adapter) {
                           return isCanonicalAddress(adapter.address)
                               && isBoundedText(adapter.name, kMaxAdapterNameUtf8Bytes);
                       })
        && std::all_of(inventory.devices.cbegin(), inventory.devices.cend(),
                       [&](const BackendDevice &device) {
                           const auto adapterIt = std::find_if(
                               inventory.adapters.cbegin(), inventory.adapters.cend(),
                               [&](const BackendAdapter &adapter) {
                                   return adapter.address == device.adapterAddress;
                               });
                           return adapterIt != inventory.adapters.cend()
                               && isCanonicalAddress(device.address)
                               && isBoundedText(device.name, kMaxDeviceNameUtf8Bytes);
                       });
    if (!inventorySane) {
        publishBackendMalformed();
        return;
    }

    Snapshot candidate = projectInventory(inventory);
    if (validateSnapshot(candidate).accepted) {
        m_inventory = inventory;
    } else {
        publishBackendMalformed();
        return;
    }

    if (m_hasInventory && candidate == m_snapshot) {
        // Equal lineage has one canonical value; a redundant republication of
        // identical content is dropped without advancing the revision.
        return;
    }
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    candidate.revision = m_snapshot.revision + 1;
    m_snapshot = candidate;
    m_hasInventory = true;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void BluetoothModel::acceptBackendResult(const quint64 generation,
                                          const quint64 operationId,
                                          const BackendOperationOutcome &outcome)
{
    if (!m_running || generation == 0 || generation != m_backendGeneration) {
        return;
    }
    const auto it = m_pending.find(operationId);
    if (it == m_pending.end()) {
        return;
    }
    const PendingOperation pending = it.value();
    m_pending.erase(it);

    OperationStatus status = OperationStatus::Failed;
    bool knownStatus = true;
    switch (outcome.status) {
    case BackendOperationStatus::Succeeded:
        status = OperationStatus::Succeeded;
        break;
    case BackendOperationStatus::Rejected:
        status = OperationStatus::Rejected;
        break;
    case BackendOperationStatus::Failed:
        status = OperationStatus::Failed;
        break;
    case BackendOperationStatus::Uncertain:
        status = OperationStatus::Uncertain;
        break;
    default:
        knownStatus = false;
        break;
    }
    const bool authorityReplaced = pending.epoch != m_snapshot.epoch;
    if (authorityReplaced) {
        status = OperationStatus::Uncertain;
    }

    OperationResult result{.kind = pending.kind,
                           .status = status,
                           .initiatingEpoch = pending.epoch,
                           .initiatingRevision = pending.revision,
                           .observedEpoch = m_snapshot.epoch,
                           .observedRevision = m_snapshot.revision,
                           .reasonCode = outcome.reasonCode,
                           .diagnostic = boundedSafeDiagnostic(outcome.diagnostic),
                           .wireValid = true};
    if (authorityReplaced) {
        result.reasonCode = QStringLiteral("authority-replaced");
        result.diagnostic.clear();
    }
    // AGENT-GUARD: AdapterBackend is an untrusted platform boundary. Never
    // copy a partially sanitized outcome to D-Bus; one invalid field replaces
    // the entire classification with a stable, protocol-valid failure.
    if (!knownStatus || !validBackendReasonCode(outcome.reasonCode)
        || !validateOperationResult(result).accepted) {
        result.status = OperationStatus::Failed;
        result.reasonCode = QStringLiteral("backend-malformed");
        result.diagnostic.clear();
    }
    Q_ASSERT(validateOperationResult(result).accepted);
    Q_EMIT operationCompleted(operationId, result);
}

} // namespace QindaQt::Bluetooth
