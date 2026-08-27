// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Audio
{
namespace
{

bool hasCapability(const Capabilities capabilities, const Capability capability)
{
    return capabilities.testFlag(capability);
}

const Device *findDevice(const Snapshot &snapshot, const Handle &handle)
{
    const auto inspect = [&](const QList<Device> &devices) -> const Device * {
        for (const Device &device : devices) {
            if (device.handle == handle) {
                return &device;
            }
        }
        return nullptr;
    };
    const Device *device = inspect(snapshot.outputs);
    return device == nullptr ? inspect(snapshot.inputs) : device;
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

} // namespace

AudioOperationCoordinator::AudioOperationCoordinator(AudioBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
    Q_ASSERT(m_backend != nullptr);
    m_snapshot.schemaVersion = kSchemaVersion;
    m_snapshot.epoch = 1;
    m_snapshot.revision = 1;
    m_snapshot.availability = Availability::Starting;
    m_snapshot.reasonCode = QStringLiteral("starting");

    connect(m_backend, &AudioBackend::snapshotReady, this,
            &AudioOperationCoordinator::acceptSnapshot);
    connect(m_backend, &AudioBackend::operationFinished, this,
            &AudioOperationCoordinator::acceptBackendResult);
}

const Snapshot &AudioOperationCoordinator::snapshot() const noexcept
{
    return m_snapshot;
}

void AudioOperationCoordinator::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_backend->start();
}

void AudioOperationCoordinator::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_backend->stop();
    makePendingUncertain(m_snapshot, QStringLiteral("service-stopped"));
}

OperationResult AudioOperationCoordinator::immediate(const OperationRequest &request,
                                                     const OperationStatus status,
                                                     const QString &reasonCode) const
{
    return {
        .kind = request.kind,
        .status = status,
        .initiatingEpoch = m_snapshot.epoch,
        .initiatingRevision = m_snapshot.revision,
        .observedEpoch = m_snapshot.epoch,
        .observedRevision = m_snapshot.revision,
        .reasonCode = reasonCode,
        .diagnostic = {},
        .wireValid = true,
    };
}

QString AudioOperationCoordinator::validateRequest(const OperationRequest &request) const
{
    if (!m_running || (m_snapshot.availability != Availability::Ready
                       && m_snapshot.availability != Availability::Degraded)) {
        return QStringLiteral("unavailable");
    }
    if (!request.primary.isValid() || request.primary.epoch != m_snapshot.epoch) {
        return QStringLiteral("stale-handle");
    }

    switch (request.kind) {
    case OperationKind::SetDefault:
        if (!hasCapability(m_snapshot.capabilities, Capability::SetDefault)) {
            return QStringLiteral("unsupported");
        }
        if (findDevice(m_snapshot, request.primary) == nullptr) {
            return QStringLiteral("stale-handle");
        }
        break;
    case OperationKind::SetVolume: {
        if (!hasCapability(m_snapshot.capabilities, Capability::SetVolume)) {
            return QStringLiteral("unsupported");
        }
        if (!std::isfinite(request.volume) || request.volume < 0.0
            || request.volume > 1.0) {
            return QStringLiteral("invalid-volume");
        }
        const Device *device = findDevice(m_snapshot, request.primary);
        const Stream *stream = findStream(m_snapshot, request.primary);
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if ((device != nullptr && !device->canSetVolume)
            || (stream != nullptr && !stream->canSetVolume)) {
            return QStringLiteral("unsupported");
        }
        break;
    }
    case OperationKind::SetMute: {
        if (!hasCapability(m_snapshot.capabilities, Capability::SetMute)) {
            return QStringLiteral("unsupported");
        }
        const Device *device = findDevice(m_snapshot, request.primary);
        const Stream *stream = findStream(m_snapshot, request.primary);
        if (device == nullptr && stream == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if ((device != nullptr && !device->canSetMute)
            || (stream != nullptr && !stream->canSetMute)) {
            return QStringLiteral("unsupported");
        }
        break;
    }
    case OperationKind::MoveStream: {
        if (!hasCapability(m_snapshot.capabilities, Capability::MoveStream)) {
            return QStringLiteral("unsupported");
        }
        if (!request.secondary.isValid() || request.secondary.epoch != m_snapshot.epoch) {
            return QStringLiteral("stale-handle");
        }
        const Stream *stream = findStream(m_snapshot, request.primary);
        const Device *target = findDevice(m_snapshot, request.secondary);
        if (stream == nullptr || target == nullptr) {
            return QStringLiteral("stale-handle");
        }
        if (!stream->canMove) {
            return QStringLiteral("unsupported");
        }
        const bool compatible = stream->direction == StreamDirection::Playback
            ? target->kind == DeviceKind::Output
            : target->kind == DeviceKind::Input;
        if (!compatible) {
            return QStringLiteral("incompatible-target");
        }
        break;
    }
    default:
        return QStringLiteral("malformed-request");
    }
    return {};
}

OperationSubmission AudioOperationCoordinator::submit(const OperationRequest &request)
{
    const QString rejection = validateRequest(request);
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

    const quint64 operationId = m_nextOperationId++;
    m_pending.insert(operationId,
                     {.kind = request.kind,
                      .epoch = m_snapshot.epoch,
                      .revision = m_snapshot.revision});
    m_backend->submit(operationId, request);
    return {.pending = true, .operationId = operationId, .immediateResult = {}};
}

void AudioOperationCoordinator::makePendingUncertain(const Snapshot &observed,
                                                      const QString &reasonCode)
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
             .observedEpoch = observed.epoch,
             .observedRevision = observed.revision,
             .reasonCode = reasonCode,
             .diagnostic = {},
             .wireValid = true});
    }
}

void AudioOperationCoordinator::acceptSnapshot(const Snapshot &snapshot)
{
    const ValidationResult validation = validateSnapshot(snapshot);
    if (!validation.accepted) {
        Snapshot unavailable = m_snapshot;
        if (unavailable.revision == std::numeric_limits<quint64>::max()) {
            return;
        }
        ++unavailable.revision;
        unavailable.availability = Availability::Degraded;
        unavailable.capabilities = {};
        unavailable.reasonCode = QStringLiteral("backend-malformed");
        unavailable.diagnostic.clear();
        unavailable.defaultOutput = {};
        unavailable.defaultInput = {};
        unavailable.outputs.clear();
        unavailable.inputs.clear();
        unavailable.streams.clear();
        makePendingUncertain(unavailable, QStringLiteral("backend-malformed"));
        m_snapshot = unavailable;
        Q_EMIT snapshotChanged(m_snapshot);
        Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
        return;
    }

    if (snapshot.epoch != m_snapshot.epoch) {
        makePendingUncertain(snapshot, QStringLiteral("authority-replaced"));
    }
    if (snapshot == m_snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void AudioOperationCoordinator::acceptBackendResult(
    const quint64 operationId, const BackendOperationOutcome &outcome)
{
    const auto it = m_pending.find(operationId);
    if (it == m_pending.end()) {
        return;
    }
    const PendingOperation pending = it.value();
    m_pending.erase(it);

    OperationStatus status = OperationStatus::Failed;
    switch (outcome.status) {
    case BackendOperationStatus::Succeeded:
        status = OperationStatus::Succeeded;
        break;
    case BackendOperationStatus::Unsupported:
        status = OperationStatus::Unsupported;
        break;
    case BackendOperationStatus::Failed:
        status = OperationStatus::Failed;
        break;
    case BackendOperationStatus::Uncertain:
        status = OperationStatus::Uncertain;
        break;
    }
    if (pending.epoch != m_snapshot.epoch) {
        status = OperationStatus::Uncertain;
    }

    Q_EMIT operationCompleted(
        operationId,
        {.kind = pending.kind,
         .status = status,
         .initiatingEpoch = pending.epoch,
         .initiatingRevision = pending.revision,
         .observedEpoch = m_snapshot.epoch,
         .observedRevision = m_snapshot.revision,
         .reasonCode = outcome.reasonCode,
         .diagnostic = boundedSafeDiagnostic(outcome.diagnostic),
         .wireValid = true});
}

} // namespace QindaQt::Audio
