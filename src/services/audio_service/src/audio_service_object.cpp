// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_service_object_p.h"

#include <QtDBus/QDBusConnection>

namespace QindaQt::Audio
{

AudioServiceObject::AudioServiceObject(AudioOperationCoordinator *coordinator,
                                       const QDBusConnection &connection,
                                       QObject *parent)
    : QObject(parent)
    , m_coordinator(coordinator)
    , m_connection(connection)
{
    Q_ASSERT(m_coordinator != nullptr);
    connect(m_coordinator, &AudioOperationCoordinator::invalidated, this,
            &AudioServiceObject::Changed);
    connect(m_coordinator, &AudioOperationCoordinator::operationCompleted, this,
            &AudioServiceObject::finishOperation);
}

Snapshot AudioServiceObject::GetSnapshot() const
{
    return m_coordinator->snapshot();
}

void AudioServiceObject::SetDefault(const Handle &device)
{
    beginOperation({.kind = OperationKind::SetDefault,
                    .primary = device,
                    .secondary = {},
                    .volume = 0.0,
                    .muted = false});
}

void AudioServiceObject::SetVolume(const Handle &target, const double volume)
{
    beginOperation({.kind = OperationKind::SetVolume,
                    .primary = target,
                    .secondary = {},
                    .volume = volume,
                    .muted = false});
}

void AudioServiceObject::SetMute(const Handle &target, const bool muted)
{
    beginOperation({.kind = OperationKind::SetMute,
                    .primary = target,
                    .secondary = {},
                    .volume = 0.0,
                    .muted = muted});
}

void AudioServiceObject::MoveStream(const Handle &stream, const Handle &device)
{
    beginOperation(
        {.kind = OperationKind::MoveStream,
         .primary = stream,
         .secondary = device,
         .volume = 0.0,
         .muted = false});
}

void AudioServiceObject::SetChannelVolumes(const Handle &target, const QVector<double> &volumes)
{
    beginOperation({.kind = OperationKind::SetChannelVolumes,
                    .primary = target,
                    .secondary = {},
                    .volume = 0.0,
                    .muted = false,
                    .channelVolumes = volumes});
}

void AudioServiceObject::CreateVirtualDevice(const quint32 kind, const QString &displayName,
                                             const quint32 channels)
{
    beginOperation({.kind = OperationKind::CreateVirtualDevice,
                    .primary = {},
                    .secondary = {},
                    .volume = 0.0,
                    .muted = false,
                    .channelVolumes = {},
                    .deviceKind = static_cast<DeviceKind>(kind),
                    .displayName = displayName,
                    .channels = channels});
}

void AudioServiceObject::RemoveVirtualDevice(const Handle &device)
{
    beginOperation({.kind = OperationKind::RemoveVirtualDevice,
                    .primary = device,
                    .secondary = {},
                    .volume = 0.0,
                    .muted = false});
}

namespace {

// Console operations (ADR-0173) are addressed by console id, not by a graph
// handle, so they share this builder rather than each spelling out the whole
// OperationRequest. The console model is what validates identity and range;
// no policy is duplicated here.
[[nodiscard]] OperationRequest consoleRequest(OperationKind kind, const QString &id)
{
    OperationRequest request;
    request.kind = kind;
    request.consoleId = id;
    return request;
}

} // namespace

void AudioServiceObject::SetStripGain(const QString &strip, const double gainDb)
{
    auto request = consoleRequest(OperationKind::SetStripGain, strip);
    request.gainDb = gainDb;
    beginOperation(request);
}

void AudioServiceObject::SetStripMute(const QString &strip, const bool muted)
{
    auto request = consoleRequest(OperationKind::SetStripMute, strip);
    request.muted = muted;
    beginOperation(request);
}

void AudioServiceObject::SetStripSolo(const QString &strip, const bool soloed)
{
    auto request = consoleRequest(OperationKind::SetStripSolo, strip);
    request.enabled = soloed;
    beginOperation(request);
}

void AudioServiceObject::SetStripMono(const QString &strip, const bool mono)
{
    auto request = consoleRequest(OperationKind::SetStripMono, strip);
    request.enabled = mono;
    beginOperation(request);
}

void AudioServiceObject::SetStripPan(const QString &strip, const double pan)
{
    auto request = consoleRequest(OperationKind::SetStripPan, strip);
    request.pan = pan;
    beginOperation(request);
}

void AudioServiceObject::SetStripTrim(const QString &strip,
                                      const QVector<double> &trimDb)
{
    auto request = consoleRequest(OperationKind::SetStripTrim, strip);
    request.channelVolumes = trimDb;
    beginOperation(request);
}

void AudioServiceObject::SetStripSend(const QString &strip, const quint32 busIndex,
                                      const bool enabled, const double gainDb)
{
    auto request = consoleRequest(OperationKind::SetStripSend, strip);
    request.busIndex = busIndex;
    request.enabled = enabled;
    request.gainDb = gainDb;
    beginOperation(request);
}

void AudioServiceObject::SetBusGain(const QString &bus, const double gainDb)
{
    auto request = consoleRequest(OperationKind::SetBusGain, bus);
    request.gainDb = gainDb;
    beginOperation(request);
}

void AudioServiceObject::SetBusMute(const QString &bus, const bool muted)
{
    auto request = consoleRequest(OperationKind::SetBusMute, bus);
    request.muted = muted;
    beginOperation(request);
}

void AudioServiceObject::SetBusMono(const QString &bus, const bool mono)
{
    auto request = consoleRequest(OperationKind::SetBusMono, bus);
    request.enabled = mono;
    beginOperation(request);
}

void AudioServiceObject::SetBusTarget(const QString &bus, const Handle &device)
{
    auto request = consoleRequest(OperationKind::SetBusTarget, bus);
    request.primary = device;
    beginOperation(request);
}

void AudioServiceObject::beginOperation(const OperationRequest &request)
{
    if (!calledFromDBus()) {
        return;
    }

    const QDBusMessage call = message();
    setDelayedReply(true);
    const OperationSubmission submission = m_coordinator->submit(request);
    if (!submission.pending) {
        m_connection.send(call.createReply(QVariant::fromValue(submission.immediateResult)));
        return;
    }

    // AGENT-GUARD: The coordinator caps all pending operations before this
    // insertion. Keeping exactly one original call per operation prevents a
    // timeout or authority replacement from being accidentally replayed.
    m_pendingReplies.insert(submission.operationId, call);
}

void AudioServiceObject::finishOperation(const quint64 operationId,
                                         const OperationResult &result)
{
    const auto it = m_pendingReplies.find(operationId);
    if (it == m_pendingReplies.end()) {
        return;
    }
    const QDBusMessage call = it.value();
    m_pendingReplies.erase(it);
    // QDBusContext is valid only during the original method invocation. The
    // retained message and explicitly owned connection are the complete async
    // reply capability; consulting QDBusContext here would dereference expired
    // call-local state.
    m_connection.send(call.createReply(QVariant::fromValue(result)));
}

} // namespace QindaQt::Audio
