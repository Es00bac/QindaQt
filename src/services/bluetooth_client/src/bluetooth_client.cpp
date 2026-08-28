// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <limits>

namespace QindaQt::Bluetooth
{
namespace
{

constexpr int kMinRetryIntervalMs = 200;
constexpr int kMaxRetryIntervalMs = 2000;

const Adapter *findAdapter(const Snapshot &snapshot, const Handle &handle)
{
    for (const Adapter &adapter : snapshot.adapters) {
        if (adapter.handle == handle) {
            return &adapter;
        }
    }
    return nullptr;
}

const Device *findDevice(const Snapshot &snapshot, const Handle &handle)
{
    for (const Device &device : snapshot.devices) {
        if (device.handle == handle) {
            return &device;
        }
    }
    return nullptr;
}

QString preflightOperation(const Snapshot &snapshot, const OperationRequest &request)
{
    if (snapshot.availability != Availability::Ready) {
        return QStringLiteral("unavailable");
    }
    if (!request.target.isValid() || request.target.epoch != snapshot.epoch) {
        return QStringLiteral("stale-handle");
    }
    switch (request.kind) {
    case OperationKind::SetAdapterPower: {
        if (!snapshot.capabilities.testFlag(Capability::SetAdapterPower)) {
            return QStringLiteral("unsupported");
        }
        return findAdapter(snapshot, request.target) == nullptr
            ? QStringLiteral("stale-handle")
            : QString{};
    }
    case OperationKind::AcquireDiscovery: {
        if (!snapshot.capabilities.testFlag(Capability::DiscoveryLease)) {
            return QStringLiteral("unsupported");
        }
        const Adapter *adapter = findAdapter(snapshot, request.target);
        if (adapter == nullptr) {
            return QStringLiteral("stale-handle");
        }
        return adapter->powered ? QString{} : QStringLiteral("adapter-off");
    }
    case OperationKind::ReleaseDiscovery: {
        if (!snapshot.capabilities.testFlag(Capability::DiscoveryLease)) {
            return QStringLiteral("unsupported");
        }
        return findAdapter(snapshot, request.target) == nullptr
            ? QStringLiteral("stale-handle")
            : QString{};
    }
    case OperationKind::Connect: {
        if (!snapshot.capabilities.testFlag(Capability::ConnectPaired)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(snapshot, request.target);
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!device->paired) {
            return QStringLiteral("not-paired");
        }
        const Adapter *adapter = findAdapter(snapshot, device->adapterHandle);
        return adapter != nullptr && adapter->powered ? QString{}
                                                      : QStringLiteral("adapter-off");
    }
    case OperationKind::Disconnect: {
        if (!snapshot.capabilities.testFlag(Capability::DisconnectPaired)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(snapshot, request.target);
        if (device == nullptr) {
            return QStringLiteral("stale-handle");
        }
        return device->connected ? QString{} : QStringLiteral("not-connected");
    }
    }
    return QStringLiteral("malformed-request");
}

} // namespace

BluetoothClient::BluetoothClient(BluetoothTransport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
    Q_ASSERT(m_transport != nullptr);
    m_fetchTimer.setSingleShot(true);
    m_operationTimer.setSingleShot(true);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(200);
    connect(&m_fetchTimer, &QTimer::timeout, this, &BluetoothClient::onFetchTimeout);
    connect(&m_operationTimer, &QTimer::timeout, this, &BluetoothClient::onOperationTimeout);
    connect(&m_retryTimer, &QTimer::timeout, this, &BluetoothClient::requestSnapshot);
    connect(m_transport, &BluetoothTransport::ownerChanged, this,
            &BluetoothClient::acceptOwner);
    connect(m_transport, &BluetoothTransport::invalidated, this,
            &BluetoothClient::acceptInvalidation);
    connect(m_transport, &BluetoothTransport::snapshotReply, this,
            &BluetoothClient::acceptSnapshotReply);
    connect(m_transport, &BluetoothTransport::operationReply, this,
            &BluetoothClient::acceptOperationReply);
}

void BluetoothClient::start()
{
    if (m_state != ClientState::Stopped) {
        return;
    }
    publishState(ClientState::Starting, QStringLiteral("discovering-owner"));
    m_transport->start();
}

void BluetoothClient::stop()
{
    if (m_state == ClientState::Stopped) {
        return;
    }
    // AGENT-GUARD: Results accepted before stop but not yet published belong to
    // the cancelled client lifetime. Drop those first, but retain the distinct
    // asynchronous Uncertain result created here for a mutation that is still
    // transport-backed. The receiver-context queue drops it on destruction.
    cancelQueuedOperationCompletions();
    completeUncertain(QStringLiteral("client-stopped"));
    m_fetchTimer.stop();
    m_operationTimer.stop();
    m_retryTimer.stop();
    m_fetchInFlight = false;
    m_refetchNeeded = false;
    m_fetchRequestId = 0;
    m_snapshot.reset();
    m_owner.clear();
    m_retryIntervalMs = kMinRetryIntervalMs;
    m_transport->stop();
    publishState(ClientState::Stopped, {});
}

ClientState BluetoothClient::state() const noexcept
{
    return m_state;
}

QString BluetoothClient::reasonCode() const
{
    return m_reasonCode;
}

QString BluetoothClient::owner() const
{
    return m_owner;
}

bool BluetoothClient::hasSnapshot() const noexcept
{
    return m_snapshot.has_value();
}

Snapshot BluetoothClient::snapshot() const
{
    return m_snapshot.value_or(Snapshot{});
}

bool BluetoothClient::operationPending() const noexcept
{
    return m_operation.has_value();
}

void BluetoothClient::setRequestTimeout(const int milliseconds)
{
    m_requestTimeoutMs = qBound(10, milliseconds, 60'000);
}

void BluetoothClient::publishState(const ClientState state, const QString &reasonCode)
{
    if (m_state == state && m_reasonCode == reasonCode) {
        return;
    }
    m_state = state;
    m_reasonCode = reasonCode;
    Q_EMIT stateChanged(m_state, m_reasonCode);
}

void BluetoothClient::publishSnapshotState(const Snapshot &snapshot)
{
    switch (snapshot.availability) {
    case Availability::Starting:
        publishState(ClientState::Starting, snapshot.reasonCode);
        break;
    case Availability::Ready:
        publishState(ClientState::Ready, snapshot.reasonCode);
        break;
    case Availability::Unavailable:
        publishState(ClientState::Unavailable, snapshot.reasonCode);
        break;
    case Availability::Degraded:
        publishState(ClientState::Degraded, snapshot.reasonCode);
        break;
    }
}

void BluetoothClient::acceptOwner(const QString &owner)
{
    if (m_state == ClientState::Stopped || owner == m_owner) {
        return;
    }
    completeUncertain(QStringLiteral("owner-replaced"));
    m_fetchTimer.stop();
    m_retryTimer.stop();
    m_fetchInFlight = false;
    m_refetchNeeded = false;
    m_fetchRequestId = 0;
    m_retryIntervalMs = kMinRetryIntervalMs;
    m_snapshot.reset();
    m_owner = owner;
    if (m_owner.isEmpty()) {
        publishState(ClientState::Unavailable, QStringLiteral("service-unavailable"));
        return;
    }
    publishState(ClientState::Starting, QStringLiteral("fetching-snapshot"));
    requestSnapshot();
}

void BluetoothClient::requestSnapshot()
{
    if (m_owner.isEmpty() || m_state == ClientState::Stopped) {
        return;
    }
    if (m_fetchInFlight) {
        m_refetchNeeded = true;
        return;
    }
    if (m_nextRequestId == 0 || m_nextRequestId == std::numeric_limits<quint64>::max()) {
        publishState(ClientState::Unavailable, QStringLiteral("request-id-exhausted"));
        return;
    }
    m_fetchInFlight = true;
    m_refetchNeeded = false;
    m_fetchRequestId = m_nextRequestId++;
    m_fetchTimer.start(m_requestTimeoutMs);
    m_transport->fetchSnapshot(m_owner, m_fetchRequestId);
}

void BluetoothClient::scheduleRefetch()
{
    if (m_retryTimer.isActive() || m_owner.isEmpty()) {
        return;
    }
    // Bounded exponential backoff: double toward the cap, reset on the next
    // accepted snapshot, so a dead service cannot spin the bus at a fixed
    // 200 ms cadence.
    m_retryTimer.start(m_retryIntervalMs);
    m_retryIntervalMs = qMin(m_retryIntervalMs * 2, kMaxRetryIntervalMs);
}

void BluetoothClient::acceptInvalidation(const QString &owner, const quint64 epoch,
                                         const quint64 revision)
{
    if (owner != m_owner || owner.isEmpty()) {
        return;
    }
    if (!m_snapshot.has_value() || epoch != m_snapshot->epoch
        || revision >= m_snapshot->revision) {
        requestSnapshot();
    }
}

void BluetoothClient::acceptSnapshotReply(const QString &owner, const quint64 requestId,
                                          const bool transportSuccess,
                                          const Snapshot &snapshot,
                                          const QString &reasonCode)
{
    if (!m_fetchInFlight || owner != m_owner || requestId != m_fetchRequestId) {
        return;
    }
    m_fetchTimer.stop();
    m_fetchInFlight = false;
    m_fetchRequestId = 0;

    const ValidationResult validation = validateSnapshot(snapshot);
    bool lineageContradiction = false;
    bool exactDuplicate = false;
    if (m_snapshot.has_value()) {
        if (snapshot.epoch < m_snapshot->epoch) {
            lineageContradiction = true;
        } else if (snapshot.epoch == m_snapshot->epoch) {
            if (snapshot.revision < m_snapshot->revision) {
                lineageContradiction = true;
            } else if (snapshot.revision == m_snapshot->revision) {
                exactDuplicate = snapshot == *m_snapshot;
                lineageContradiction = !exactDuplicate;
            }
        }
    }
    if (!transportSuccess || !validation.accepted || lineageContradiction) {
        // AGENT-GUARD: A failed fetch revokes mutation authority. The
        // retained snapshot can no longer be proven current, so it is
        // dropped and any dispatched operation completes as Uncertain
        // instead of being authorized by possibly stale state.
        m_snapshot.reset();
        completeUncertain(QStringLiteral("snapshot-unavailable"));
        m_retryIntervalMs = kMinRetryIntervalMs;
        publishState(ClientState::Unavailable,
                     !transportSuccess ? reasonCode : QStringLiteral("malformed-snapshot"));
        scheduleRefetch();
        return;
    }

    if (exactDuplicate) {
        m_retryIntervalMs = kMinRetryIntervalMs;
        publishSnapshotState(snapshot);
        if (m_refetchNeeded) {
            requestSnapshot();
        }
        return;
    }
    const bool authorityReplaced = m_snapshot.has_value()
        && snapshot.epoch != m_snapshot->epoch;
    m_snapshot = snapshot;
    m_retryIntervalMs = kMinRetryIntervalMs;
    publishSnapshotState(snapshot);
    Q_EMIT snapshotChanged(snapshot);
    if (authorityReplaced) {
        // AGENT-CONTRACT: Epochs are strictly increasing only within this
        // exact service owner. A new accepted epoch retires the dispatched
        // mutation immediately; a delayed old-epoch reply is never allowed to
        // restore success or trigger a replay.
        completeUncertain(QStringLiteral("authority-replaced"));
    }
    if (m_refetchNeeded) {
        requestSnapshot();
    }
}

OperationResult BluetoothClient::localResult(const OperationRequest &request,
                                             const OperationStatus status,
                                             const QString &reasonCode) const
{
    const quint64 epoch = m_snapshot.has_value() ? m_snapshot->epoch : 1;
    const quint64 revision = m_snapshot.has_value() ? m_snapshot->revision : 1;
    return {.kind = request.kind,
            .status = status,
            .initiatingEpoch = epoch,
            .initiatingRevision = revision,
            .observedEpoch = epoch,
            .observedRevision = revision,
            .reasonCode = reasonCode,
            .diagnostic = {},
            .wireValid = true};
}

quint64 BluetoothClient::beginOperation(const OperationRequest &request)
{
    if (m_nextRequestId == 0 || m_nextRequestId == std::numeric_limits<quint64>::max()) {
        return 0;
    }
    const quint64 requestId = m_nextRequestId++;
    if (m_operation.has_value()) {
        queueOperationCompletion(
            requestId,
            localResult(request, OperationStatus::Busy, QStringLiteral("operation-busy")));
        return requestId;
    }
    if (m_owner.isEmpty() || !m_snapshot.has_value()) {
        queueOperationCompletion(
            requestId,
            localResult(request, OperationStatus::Rejected, QStringLiteral("unavailable")));
        return requestId;
    }
    const QString rejection = preflightOperation(*m_snapshot, request);
    if (!rejection.isEmpty()) {
        queueOperationCompletion(
            requestId,
            localResult(request,
                        rejection == QStringLiteral("unsupported")
                            ? OperationStatus::Unsupported
                            : OperationStatus::Rejected,
                        rejection));
        return requestId;
    }    m_operation = PendingOperation{.requestId = requestId,
                                   .request = request,
                                   .epoch = m_snapshot->epoch,
                                   .revision = m_snapshot->revision};
    m_operationTimer.start(m_requestTimeoutMs);
    m_transport->submitOperation(m_owner, requestId, request);
    return requestId;
}

quint64 BluetoothClient::setAdapterPower(const Handle &adapter, const bool powered)
{
    return beginOperation(
        {.kind = OperationKind::SetAdapterPower, .target = adapter, .powered = powered});
}

quint64 BluetoothClient::acquireDiscovery(const Handle &adapter)
{
    return beginOperation(
        {.kind = OperationKind::AcquireDiscovery, .target = adapter, .powered = false});
}

quint64 BluetoothClient::releaseDiscovery(const Handle &adapter)
{
    return beginOperation(
        {.kind = OperationKind::ReleaseDiscovery, .target = adapter, .powered = false});
}

quint64 BluetoothClient::connectDevice(const Handle &device)
{
    return beginOperation(
        {.kind = OperationKind::Connect, .target = device, .powered = false});
}

quint64 BluetoothClient::disconnectDevice(const Handle &device)
{
    return beginOperation(
        {.kind = OperationKind::Disconnect, .target = device, .powered = false});
}

void BluetoothClient::completeUncertain(const QString &reasonCode)
{
    if (!m_operation.has_value()) {
        return;
    }
    const PendingOperation pending = *m_operation;
    m_operation.reset();
    m_operationTimer.stop();
    const quint64 observedEpoch = m_snapshot.has_value() ? m_snapshot->epoch : pending.epoch;
    const quint64 observedRevision = m_snapshot.has_value() ? m_snapshot->revision
                                                            : pending.revision;
    queueOperationCompletion(
        pending.requestId,
        {.kind = pending.request.kind,
         .status = OperationStatus::Uncertain,
         .initiatingEpoch = pending.epoch,
         .initiatingRevision = pending.revision,
         .observedEpoch = observedEpoch,
         .observedRevision = observedRevision,
         .reasonCode = reasonCode,
         .diagnostic = {},
         .wireValid = true});
}

void BluetoothClient::acceptOperationReply(const QString &owner, const quint64 requestId,
                                           const bool transportSuccess,
                                           const OperationResult &result,
                                           const QString &reasonCode)
{
    if (!m_operation.has_value() || owner != m_owner
        || requestId != m_operation->requestId) {
        return;
    }
    const PendingOperation pending = *m_operation;
    m_operation.reset();
    m_operationTimer.stop();

    const ValidationResult validation = validateOperationResult(result);
    const bool exactInitiator = result.kind == pending.request.kind
        && result.initiatingEpoch == pending.epoch
        && result.initiatingRevision == pending.revision;
    const bool currentSuccessLineage = result.status != OperationStatus::Succeeded
        || (m_snapshot.has_value() && result.observedEpoch == m_snapshot->epoch);
    if (!transportSuccess || !validation.accepted || !exactInitiator
        || !currentSuccessLineage) {
        queueOperationCompletion(
            requestId,
            {.kind = pending.request.kind,
             .status = OperationStatus::Uncertain,
             .initiatingEpoch = pending.epoch,
             .initiatingRevision = pending.revision,
             .observedEpoch = m_snapshot.has_value() ? m_snapshot->epoch : pending.epoch,
             .observedRevision = m_snapshot.has_value() ? m_snapshot->revision
                                                        : pending.revision,
             .reasonCode = transportSuccess ? QStringLiteral("malformed-result")
                                            : reasonCode,
             .diagnostic = {},
             .wireValid = true});
        requestSnapshot();
        return;
    }

    queueOperationCompletion(requestId, result);
    requestSnapshot();
}

void BluetoothClient::onFetchTimeout()
{
    if (!m_fetchInFlight) {
        return;
    }
    m_fetchInFlight = false;
    m_fetchRequestId = 0;
    // AGENT-GUARD: A timed-out fetch leaves current state unproven; revoke
    // mutation authority exactly as for a failed fetch.
    m_snapshot.reset();
    completeUncertain(QStringLiteral("snapshot-timeout"));
    publishState(ClientState::Unavailable, QStringLiteral("snapshot-timeout"));
    scheduleRefetch();
}

void BluetoothClient::onOperationTimeout()
{
    completeUncertain(QStringLiteral("operation-timeout"));
    requestSnapshot();
}

} // namespace QindaQt::Bluetooth
