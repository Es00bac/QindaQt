// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_client/audio_client.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <cmath>
#include <limits>

namespace QindaQt::Audio
{
namespace
{

const Device *findDevice(const Snapshot &snapshot, const Handle &handle)
{
    for (const Device &device : snapshot.outputs) {
        if (device.handle == handle) {
            return &device;
        }
    }
    for (const Device &device : snapshot.inputs) {
        if (device.handle == handle) {
            return &device;
        }
    }
    return nullptr;
}

const Stream *findStream(const Snapshot &snapshot, const Handle &handle)
{
    for (const Stream &stream : snapshot.streams) {
        if (stream.handle == handle) {
            return &stream;
        }
    }
    return nullptr;
}

QString preflightOperation(const Snapshot &snapshot, const OperationRequest &request)
{
    if (snapshot.availability != Availability::Ready
        && snapshot.availability != Availability::Degraded) {
        return QStringLiteral("unavailable");
    }
    if (!request.primary.isValid() || request.primary.epoch != snapshot.epoch) {
        return QStringLiteral("stale-handle");
    }
    const Device *device = findDevice(snapshot, request.primary);
    const Stream *stream = findStream(snapshot, request.primary);
    switch (request.kind) {
    case OperationKind::SetDefault:
        if (!snapshot.capabilities.testFlag(Capability::SetDefault)) {
            return QStringLiteral("unsupported");
        }
        return device == nullptr ? QStringLiteral("stale-handle") : QString{};
    case OperationKind::SetVolume:
        if (!std::isfinite(request.volume) || request.volume < 0.0
            || request.volume > 1.0) {
            return QStringLiteral("invalid-volume");
        }
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::SetVolume)
            || (device != nullptr && !device->canSetVolume)
            || (stream != nullptr && !stream->canSetVolume)) {
            return QStringLiteral("unsupported");
        }
        return {};
    case OperationKind::SetMute:
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::SetMute)
            || (device != nullptr && !device->canSetMute)
            || (stream != nullptr && !stream->canSetMute)) {
            return QStringLiteral("unsupported");
        }
        return {};
    case OperationKind::MoveStream: {
        if (!request.secondary.isValid() || request.secondary.epoch != snapshot.epoch) {
            return QStringLiteral("stale-handle");
        }
        const Device *target = findDevice(snapshot, request.secondary);
        if (stream == nullptr || target == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!snapshot.capabilities.testFlag(Capability::MoveStream)
            || !stream->canMove) {
            return QStringLiteral("unsupported");
        }
        const bool compatible = stream->direction == StreamDirection::Playback
            ? target->kind == DeviceKind::Output
            : target->kind == DeviceKind::Input;
        return compatible ? QString{} : QStringLiteral("incompatible-target");
    }
    }
    return QStringLiteral("malformed-request");
}

} // namespace

AudioClient::AudioClient(AudioTransport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
    Q_ASSERT(m_transport != nullptr);
    m_fetchTimer.setSingleShot(true);
    m_operationTimer.setSingleShot(true);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(200);
    connect(&m_fetchTimer, &QTimer::timeout, this, &AudioClient::onFetchTimeout);
    connect(&m_operationTimer, &QTimer::timeout, this, &AudioClient::onOperationTimeout);
    connect(&m_retryTimer, &QTimer::timeout, this, &AudioClient::requestSnapshot);
    connect(m_transport, &AudioTransport::ownerChanged, this, &AudioClient::acceptOwner);
    connect(m_transport, &AudioTransport::invalidated, this,
            &AudioClient::acceptInvalidation);
    connect(m_transport, &AudioTransport::snapshotReply, this,
            &AudioClient::acceptSnapshotReply);
    connect(m_transport, &AudioTransport::operationReply, this,
            &AudioClient::acceptOperationReply);
}

void AudioClient::start()
{
    if (m_state != ClientState::Stopped) {
        return;
    }
    publishState(ClientState::Starting, QStringLiteral("discovering-owner"));
    m_transport->start();
}

void AudioClient::stop()
{
    if (m_state == ClientState::Stopped) {
        return;
    }
    completeUncertain(QStringLiteral("client-stopped"));
    m_fetchTimer.stop();
    m_operationTimer.stop();
    m_retryTimer.stop();
    m_fetchInFlight = false;
    m_refetchNeeded = false;
    m_fetchRequestId = 0;
    m_snapshot.reset();
    m_owner.clear();
    m_transport->stop();
    publishState(ClientState::Stopped, {});
}

ClientState AudioClient::state() const noexcept
{
    return m_state;
}

QString AudioClient::reasonCode() const
{
    return m_reasonCode;
}

QString AudioClient::owner() const
{
    return m_owner;
}

bool AudioClient::hasSnapshot() const noexcept
{
    return m_snapshot.has_value();
}

Snapshot AudioClient::snapshot() const
{
    return m_snapshot.value_or(Snapshot{});
}

bool AudioClient::operationPending() const noexcept
{
    return m_operation.has_value();
}

void AudioClient::setRequestTimeout(const int milliseconds)
{
    m_requestTimeoutMs = qBound(10, milliseconds, 60'000);
}

void AudioClient::publishState(const ClientState state, const QString &reasonCode)
{
    if (m_state == state && m_reasonCode == reasonCode) {
        return;
    }
    m_state = state;
    m_reasonCode = reasonCode;
    Q_EMIT stateChanged(m_state, m_reasonCode);
}

void AudioClient::acceptOwner(const QString &owner)
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
    m_snapshot.reset();
    m_owner = owner;
    if (m_owner.isEmpty()) {
        publishState(ClientState::Unavailable, QStringLiteral("service-unavailable"));
        return;
    }
    publishState(ClientState::Starting, QStringLiteral("fetching-snapshot"));
    requestSnapshot();
}

void AudioClient::requestSnapshot()
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

void AudioClient::scheduleRefetch()
{
    if (!m_retryTimer.isActive() && !m_owner.isEmpty()) {
        m_retryTimer.start();
    }
}

void AudioClient::acceptInvalidation(const QString &owner, const quint64 epoch,
                                     const quint64 revision)
{
    if (owner != m_owner || owner.isEmpty()) {
        return;
    }
    if (!m_snapshot.has_value() || epoch != m_snapshot->epoch
        || revision > m_snapshot->revision) {
        requestSnapshot();
    }
}

void AudioClient::acceptSnapshotReply(const QString &owner, const quint64 requestId,
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
    const bool regressed = m_snapshot.has_value() && snapshot.epoch == m_snapshot->epoch
        && snapshot.revision < m_snapshot->revision;
    if (!transportSuccess || !validation.accepted || regressed) {
        publishState(ClientState::Unavailable,
                     !transportSuccess ? reasonCode : QStringLiteral("malformed-snapshot"));
        scheduleRefetch();
        return;
    }

    m_snapshot = snapshot;
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
    Q_EMIT snapshotChanged(snapshot);
    if (m_refetchNeeded) {
        requestSnapshot();
    }
}

OperationResult AudioClient::localResult(const OperationRequest &request,
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

quint64 AudioClient::beginOperation(const OperationRequest &request)
{
    if (m_nextRequestId == 0 || m_nextRequestId == std::numeric_limits<quint64>::max()) {
        return 0;
    }
    const quint64 requestId = m_nextRequestId++;
    if (m_operation.has_value()) {
        Q_EMIT operationCompleted(
            requestId,
            localResult(request, OperationStatus::Busy, QStringLiteral("operation-busy")));
        return requestId;
    }
    if (m_owner.isEmpty() || !m_snapshot.has_value()) {
        Q_EMIT operationCompleted(
            requestId,
            localResult(request, OperationStatus::Rejected, QStringLiteral("unavailable")));
        return requestId;
    }
    const QString rejection = preflightOperation(*m_snapshot, request);
    if (!rejection.isEmpty()) {
        Q_EMIT operationCompleted(
            requestId,
            localResult(request,
                        rejection == QStringLiteral("unsupported")
                            ? OperationStatus::Unsupported
                            : OperationStatus::Rejected,
                        rejection));
        return requestId;
    }

    m_operation = PendingOperation{.requestId = requestId,
                                   .request = request,
                                   .epoch = m_snapshot->epoch,
                                   .revision = m_snapshot->revision};
    m_operationTimer.start(m_requestTimeoutMs);
    m_transport->submitOperation(m_owner, requestId, request);
    return requestId;
}

quint64 AudioClient::setDefault(const Handle &device)
{
    return beginOperation({.kind = OperationKind::SetDefault,
                           .primary = device,
                           .secondary = {},
                           .volume = 0.0,
                           .muted = false});
}

quint64 AudioClient::setVolume(const Handle &target, const double volume)
{
    return beginOperation(
        {.kind = OperationKind::SetVolume,
         .primary = target,
         .secondary = {},
         .volume = volume,
         .muted = false});
}

quint64 AudioClient::setMute(const Handle &target, const bool muted)
{
    return beginOperation(
        {.kind = OperationKind::SetMute,
         .primary = target,
         .secondary = {},
         .volume = 0.0,
         .muted = muted});
}

quint64 AudioClient::moveStream(const Handle &stream, const Handle &device)
{
    return beginOperation(
        {.kind = OperationKind::MoveStream,
         .primary = stream,
         .secondary = device,
         .volume = 0.0,
         .muted = false});
}

void AudioClient::completeUncertain(const QString &reasonCode)
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
    Q_EMIT operationCompleted(
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

void AudioClient::acceptOperationReply(const QString &owner, const quint64 requestId,
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
    if (!transportSuccess || !validation.accepted || !exactInitiator) {
        Q_EMIT operationCompleted(
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

    Q_EMIT operationCompleted(requestId, result);
    requestSnapshot();
}

void AudioClient::onFetchTimeout()
{
    if (!m_fetchInFlight) {
        return;
    }
    m_fetchInFlight = false;
    m_fetchRequestId = 0;
    publishState(ClientState::Unavailable, QStringLiteral("snapshot-timeout"));
    scheduleRefetch();
}

void AudioClient::onOperationTimeout()
{
    completeUncertain(QStringLiteral("operation-timeout"));
    requestSnapshot();
}

} // namespace QindaQt::Audio
