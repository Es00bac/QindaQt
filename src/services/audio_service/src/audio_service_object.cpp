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
