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

bool validBackendReasonCode(const QString &reasonCode)
{
    return isStructuredReasonCode(reasonCode);
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
    if (epochSeed != 0) {
        m_snapshot.epoch = epochSeed;
        m_lastIssuedEpoch = epochSeed;
    } else {
        m_snapshot.epoch = advanceEpoch();
    }
    m_snapshot.revision = 1;
    m_snapshot.availability = Availability::Starting;
    m_snapshot.reasonCode = QStringLiteral("starting");

    connect(m_backend, &AdapterBackend::inventoryChanged, this,
            &BluetoothModel::acceptInventory);
    connect(m_backend, &AdapterBackend::operationFinished, this,
            &BluetoothModel::acceptBackendResult);
}

Snapshot BluetoothModel::snapshot() const
{
    return m_snapshot;
}

void BluetoothModel::start()
{
    if (m_running) {
        return;
    }
    if (m_startedOnce) {
        // AGENT-GUARD: Reuse always advances the epoch, even when no
        // inventory was ever accepted. Otherwise a stop/start before the
        // first publication would silently reuse the epoch and could reissue
        // handles an earlier run already issued.
        const quint64 epoch = advanceEpoch();
        if (epoch == 0) {
            return;
        }
        Snapshot restarting;
        restarting.schemaVersion = kSchemaVersion;
        restarting.epoch = epoch;
        restarting.revision = 1;
        restarting.availability = Availability::Starting;
        restarting.reasonCode = QStringLiteral("backend-restarting");
        m_inventory = BackendInventory{};
        m_snapshot = restarting;
        m_hasInventory = false;
        Q_EMIT snapshotChanged(m_snapshot);
        Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
    }
    const quint64 generation = m_backend->start();
    if (generation == 0) {
        return;
    }
    m_backendGeneration = generation;
    m_running = true;
    m_startedOnce = true;
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
    // AGENT-GUARD: Epoch uniqueness rests on 64 bits of system entropy mixed
    // with wall clock, not on eight random bits; a same-millisecond process
    // pair would otherwise collide with probability 1/256. The strict
    // monotone floor keeps a reused model past every epoch it ever issued
    // even across a regressing clock.
    const quint64 random = QRandomGenerator::system()->generate64();
    const quint64 clock = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    quint64 candidate = random ^ clock;
    if (candidate == 0) {
        candidate = 1;
    }
    if (m_lastIssuedEpoch != 0) {
        if (m_lastIssuedEpoch == std::numeric_limits<quint64>::max()) {
            return 0;
        }
        if (candidate <= m_lastIssuedEpoch) {
            candidate = m_lastIssuedEpoch + 1;
        }
    }
    m_lastIssuedEpoch = candidate;
    return candidate;
}

bool BluetoothModel::safeCallerId(const QString &callerId)
{
    if (callerId.isEmpty() || !isBoundedText(callerId, kMaxCallerIdUtf8Bytes)) {
        return false;
    }
    if (!callerId.startsWith(QLatin1Char(':'))) {
        return false;
    }
    for (const QChar character : callerId) {
        const char latin = character.toLatin1();
        const bool validChar = (latin >= 'a' && latin <= 'z')
            || (latin >= 'A' && latin <= 'Z')
            || (latin >= '0' && latin <= '9')
            || latin == ':' || latin == '.' || latin == '_' || latin == '-';
        if (!validChar) {
            return false;
        }
    }
    return true;
}

quint32 BluetoothModel::adapterLeaseTotal(const BackendInventory &inventory,
                                          const QString &adapterAddress)
{
    quint32 total = 0;
    for (const BackendLease &lease : inventory.leases) {
        if (lease.adapterAddress == adapterAddress) {
            total += lease.refcount;
        }
    }
    return total;
}

quint32 BluetoothModel::totalLeases(const BackendInventory &inventory)
{
    quint32 total = 0;
    for (const BackendLease &lease : inventory.leases) {
        total += lease.refcount;
    }
    return total;
}

qsizetype BluetoothModel::pendingLeaseCount(const QString &adapterAddress) const
{
    qsizetype net = 0;
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
        if (it->adapterAddress != adapterAddress) {
            continue;
        }
        if (it->kind == OperationKind::AcquireDiscovery) {
            ++net;
        } else if (it->kind == OperationKind::ReleaseDiscovery) {
            --net;
        }
    }
    return qMax<qsizetype>(net, 0);
}

qsizetype BluetoothModel::pendingLeaseCountTotal() const
{
    qsizetype net = 0;
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
        if (it->kind == OperationKind::AcquireDiscovery) {
            ++net;
        } else if (it->kind == OperationKind::ReleaseDiscovery) {
            --net;
        }
    }
    return qMax<qsizetype>(net, 0);
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
    if (!safeCallerId(callerId)) {
        return QStringLiteral("malformed-caller");
    }
    if (!request.target.isValid() || request.target.epoch != m_snapshot.epoch) {
        return QStringLiteral("stale-handle");
    }
    if (!m_running || m_snapshot.availability != Availability::Ready) {
        return QStringLiteral("unavailable");
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
        // AGENT-GUARD: Bounds must include lease operations dispatched but
        // not yet completed; otherwise concurrently dispatched acquisitions
        // overrun the advertised lease caps before the backend publishes.
        const qsizetype projectedAdapter =
            qsizetype(adapterLeaseTotal(m_inventory, adapter->address))
            + pendingLeaseCount(adapter->address);
        const qsizetype projectedTotal = qsizetype(totalLeases(m_inventory))
            + pendingLeaseCountTotal();
        if (projectedAdapter >= kMaxDiscoveryLeasesPerAdapter
            || projectedTotal >= kMaxDiscoveryLeasesTotal) {
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
        if (device->connected) {
            return QStringLiteral("already-connected");
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
    PendingOperation pending{.kind = request.kind,
                             .epoch = m_snapshot.epoch,
                             .revision = m_snapshot.revision,
                             .adapterAddress = {}};
    if (request.kind == OperationKind::AcquireDiscovery
        || request.kind == OperationKind::ReleaseDiscovery) {
        if (const Adapter *adapter = findAdapter(request.target.serial);
            adapter != nullptr) {
            pending.adapterAddress = adapter->address;
        }
    }
    const quint64 operationId = m_nextOperationId++;
    m_pending.insert(operationId, pending);

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
