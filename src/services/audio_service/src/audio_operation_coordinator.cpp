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

bool validBackendReasonCode(const QString &reasonCode)
{
    if (reasonCode.isEmpty()
        || !isBoundedText(reasonCode, kMaxReasonCodeUtf8Bytes)) {
        return false;
    }
    for (const QChar character : reasonCode) {
        const bool lowerAscii = character >= QLatin1Char('a')
            && character <= QLatin1Char('z');
        const bool digit = character >= QLatin1Char('0')
            && character <= QLatin1Char('9');
        if (!lowerAscii && !digit && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
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
    const quint64 generation = m_backend->start();
    if (generation == 0) {
        return;
    }
    m_backendGeneration = generation;
    m_running = true;
    if (m_hasBackendSnapshot) {
        publishRestartingSnapshot();
    }
}

void AudioOperationCoordinator::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_backendGeneration = 0;
    m_backend->stop();
    makePendingUncertain(m_snapshot, QStringLiteral("service-stopped"));
}

void AudioOperationCoordinator::publishRestartingSnapshot()
{
    if (m_snapshot.revision == std::numeric_limits<quint64>::max()) {
        return;
    }
    Snapshot restarting;
    restarting.schemaVersion = kSchemaVersion;
    restarting.epoch = m_snapshot.epoch;
    restarting.revision = m_snapshot.revision + 1;
    restarting.availability = Availability::Starting;
    restarting.reasonCode = QStringLiteral("backend-restarting");
    m_snapshot = restarting;
    m_minimumRestartEpoch = m_snapshot.epoch == std::numeric_limits<quint64>::max()
        ? m_snapshot.epoch
        : m_snapshot.epoch + 1;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
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

void AudioOperationCoordinator::acceptSnapshot(const quint64 generation,
                                                const Snapshot &snapshot)
{
    // AGENT-GUARD: stop() and every later start supersede already queued backend
    // values. Check the run before validation so stale malformed data cannot
    // mutate even the fail-closed projection.
    if (!m_running || generation == 0 || generation != m_backendGeneration) {
        return;
    }
    // Backend epochs are numerically monotonic only within this resident
    // backend object (AudioBackend's contract). Apply the cheap lineage fence
    // before payload handling so an old callback cannot degrade current state.
    if (m_minimumRestartEpoch != 0 && snapshot.epoch < m_minimumRestartEpoch) {
        return;
    }
    if (m_hasBackendSnapshot) {
        if (snapshot.epoch < m_snapshot.epoch) {
            return;
        }
        if (snapshot.epoch == m_snapshot.epoch) {
            if (snapshot.revision < m_snapshot.revision) {
                return;
            }
            if (snapshot.revision == m_snapshot.revision) {
                // Equal lineage has one canonical value. An identical callback
                // is harmless; changed content is contradictory and is dropped.
                return;
            }
        }
    }

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
        m_hasBackendSnapshot = true;
        Q_EMIT snapshotChanged(m_snapshot);
        Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
        return;
    }

    if (m_hasBackendSnapshot && snapshot.epoch != m_snapshot.epoch) {
        makePendingUncertain(snapshot, QStringLiteral("authority-replaced"));
    }
    m_snapshot = snapshot;
    m_hasBackendSnapshot = true;
    m_minimumRestartEpoch = 0;
    Q_EMIT snapshotChanged(m_snapshot);
    Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void AudioOperationCoordinator::acceptBackendResult(
    const quint64 generation, const quint64 operationId,
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
    case BackendOperationStatus::Unsupported:
        status = OperationStatus::Unsupported;
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
                           .diagnostic = outcome.diagnostic,
                           .wireValid = true};
    if (authorityReplaced) {
        result.reasonCode = QStringLiteral("authority-replaced");
        result.diagnostic.clear();
    }
    // AGENT-GUARD: AudioBackend is an untrusted platform boundary. Never copy a
    // partially sanitized outcome to D-Bus; one invalid field replaces the
    // entire classification with a stable, protocol-valid failure.
    if (!knownStatus || !validBackendReasonCode(outcome.reasonCode)
        || !validateOperationResult(result).accepted) {
        result.status = OperationStatus::Failed;
        result.reasonCode = QStringLiteral("backend-malformed");
        result.diagnostic.clear();
    }
    Q_ASSERT(validateOperationResult(result).accepted);
    Q_EMIT operationCompleted(operationId, result);
}

} // namespace QindaQt::Audio
