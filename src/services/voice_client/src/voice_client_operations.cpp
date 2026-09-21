// SPDX-License-Identifier: LGPL-3.0-or-later

// Intent submission and completion for VoiceClient. Split from the lifecycle
// file so the ordering rules and the intent rules stay separately readable.

#include <qindaqt/services/voice_client/voice_client.h>

#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <QtCore/QMetaObject>

#include <limits>

namespace QindaQt::Services::Voice {

quint64 VoiceClient::begin(const OperationKind kind, const QString &providerId,
                           const bool enable)
{
    if (m_nextToken == 0 || m_nextToken == std::numeric_limits<quint64>::max()) {
        return 0;
    }
    const quint64 token = m_nextToken++;
    if (m_operation.has_value()) {
        complete(localResult(kind, token, OperationStatus::Busy,
                             QStringLiteral("operation-busy")));
        return token;
    }
    if (m_owner.isEmpty() || !m_snapshot.has_value()) {
        complete(localResult(kind, token, OperationStatus::Rejected,
                             QStringLiteral("unavailable")));
        return token;
    }
    // Refusing an unadvertised intent here keeps a provider from having to
    // defend against controls the shell should never have offered.
    if (!capabilityForKind(kind, m_snapshot->capabilities)) {
        complete(localResult(kind, token, OperationStatus::Rejected,
                             QStringLiteral("unsupported-capability")));
        return token;
    }
    const OperationRequest request{.kind = kind,
                                   .requestId = token,
                                   .expectedRevision = m_snapshot->revision,
                                   .providerId = providerId,
                                   .enable = enable};
    const ValidationResult validation = validateOperationRequest(request);
    if (!validation.accepted) {
        complete(localResult(kind, token, OperationStatus::Rejected,
                             validation.reasonCode));
        return token;
    }
    m_operation = Pending{.token = token, .request = request};
    m_operationTimer.start(m_timeoutMs);
    m_transport->submitOperation(m_owner, token, request);
    return token;
}

quint64 VoiceClient::startDictation() { return begin(OperationKind::StartDictation, {}, false); }
quint64 VoiceClient::startCommand() { return begin(OperationKind::StartCommand, {}, false); }
quint64 VoiceClient::finish() { return begin(OperationKind::Finish, {}, false); }
quint64 VoiceClient::cancel() { return begin(OperationKind::Cancel, {}, false); }
quint64 VoiceClient::retry() { return begin(OperationKind::Retry, {}, false); }
quint64 VoiceClient::undo() { return begin(OperationKind::Undo, {}, false); }
quint64 VoiceClient::copyLast() { return begin(OperationKind::CopyLast, {}, false); }

quint64 VoiceClient::setProvider(const QString &providerId)
{
    return begin(OperationKind::SetProvider, providerId, false);
}

quint64 VoiceClient::setEnabled(const bool enabled)
{
    return begin(OperationKind::SetEnabled, {}, enabled);
}

OperationResult VoiceClient::localResult(const OperationKind kind, const quint64 requestId,
                                         const OperationStatus status,
                                         const QString &reasonCode) const
{
    // A locally decided result still has to satisfy validateOperationResult, so
    // an absent snapshot borrows the lowest legal revision rather than zero.
    const quint64 revision = m_snapshot.has_value() ? m_snapshot->revision : 1;
    return {.kind = kind,
            .status = status,
            .requestId = requestId,
            .initiatingRevision = revision,
            .observedRevision = revision,
            .reasonCode = reasonCode};
}

void VoiceClient::complete(OperationResult result)
{
    // Queued so a caller that submits from a slot never re-enters this client
    // before begin() has returned its request id.
    QMetaObject::invokeMethod(this, [this, result] {
        Q_EMIT operationCompleted(result.requestId, result);
    }, Qt::QueuedConnection);
}

void VoiceClient::completeUncertain(const QString &reasonCode)
{
    if (!m_operation.has_value()) {
        return;
    }
    const Pending pending = *m_operation;
    m_operation.reset();
    m_operationTimer.stop();
    complete(localResult(pending.request.kind, pending.request.requestId,
                         OperationStatus::Uncertain, reasonCode));
}

} // namespace QindaQt::Services::Voice
