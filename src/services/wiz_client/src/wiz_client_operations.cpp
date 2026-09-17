// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_client/wiz_client.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"
#include "qindaqt/services/wiz_protocol/wiz_messages.h"
#include "qindaqt/services/wiz_protocol/wiz_validation.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>

namespace QindaQt::Wiz
{
namespace
{

// A deep queue would let a dragged slider bank up minutes of stale intents.
// Past this, the caller is told the stack is busy rather than silently
// promising a change that will arrive long after the user let go.
constexpr int maximumQueuedOperations = 32;

} // namespace

bool WizClient::operationPending() const noexcept
{
    return !m_queue.isEmpty();
}

void WizClient::deliver(const OperationResult &result, const quint64 requestId)
{
    Q_EMIT operationCompleted(requestId, result);
}

void WizClient::completeAsynchronously(const quint64 requestId,
                                     const OperationRequest &request,
                                     const OperationStatus status,
                                     const QString &reasonCode)
{
    OperationResult result;
    result.kind = request.kind;
    result.status = status;
    result.targetMac = request.targetMac;
    result.initiatingEpoch = m_model.epoch();
    result.initiatingRevision = m_model.revision();
    result.observedEpoch = m_model.epoch();
    result.observedRevision = m_model.revision();
    result.reasonCode = reasonCode;
    // AGENT-GUARD: completion must never be emitted from inside dispatch().
    // A caller that stores the request id after the call would otherwise miss
    // its own result and wait forever on an operation that already finished.
    QPointer<WizClient> self(this);
    QTimer::singleShot(0, this, [self, requestId, result]() {
        if (!self.isNull()) {
            self->deliver(result, requestId);
        }
    });
}

quint64 WizClient::dispatch(const OperationRequest &request)
{
    const quint64 requestId = m_nextRequestId++;

    if (m_state != ClientState::Ready) {
        completeAsynchronously(requestId, request, OperationStatus::Rejected,
                             QStringLiteral("client-not-ready"));
        return requestId;
    }

    if (request.kind == OperationKind::Discover) {
        sendDiscovery();
        completeAsynchronously(requestId, request, OperationStatus::Succeeded, QString());
        return requestId;
    }

    if (request.kind == OperationKind::Refresh && request.targetMac.isEmpty()) {
        for (const QString &mac : m_model.knownMacs()) {
            pollDevice(mac);
        }
        completeAsynchronously(requestId, request, OperationStatus::Succeeded, QString());
        return requestId;
    }

    const QString mac = normalizeMac(request.targetMac);
    const auto device = m_model.device(mac);
    if (mac.isEmpty() || !device.has_value()) {
        completeAsynchronously(requestId, request, OperationStatus::Rejected,
                             QStringLiteral("unknown-device"));
        return requestId;
    }

    if (m_queue.size() >= maximumQueuedOperations) {
        completeAsynchronously(requestId, request, OperationStatus::Busy,
                             QStringLiteral("queue-full"));
        return requestId;
    }

    PendingOperation operation;
    operation.requestId = requestId;
    operation.kind = request.kind;
    operation.mac = mac;
    operation.address = device->identity.address;
    operation.initiatingEpoch = m_model.epoch();
    operation.initiatingRevision = m_model.revision();

    if (request.kind == OperationKind::Refresh) {
        operation.datagram = encodeGetPilot();
    } else {
        const ValidatedRequest validated = validateStateRequest(*device, request.state);
        if (!validated.accepted()) {
            const OperationStatus status =
                validated.outcome == ValidationOutcome::Unsupported
                ? OperationStatus::Unsupported
                : OperationStatus::Rejected;
            completeAsynchronously(requestId, request, status, validated.reasonCode);
            return requestId;
        }
        operation.datagram = encodeSetPilot(validated.request);
        if (operation.datagram.isEmpty()) {
            completeAsynchronously(requestId, request, OperationStatus::Rejected,
                                 QStringLiteral("empty-request"));
            return requestId;
        }
    }

    m_queue.append(operation);
    beginNextOperation();
    return requestId;
}

void WizClient::beginNextOperation()
{
    if (m_operationInFlight || m_queue.isEmpty()) {
        return;
    }
    PendingOperation &operation = m_queue.first();
    // Re-read the endpoint: the device may have moved between being queued and
    // being sent.
    const auto endpoint = m_model.endpoint(operation.mac);
    if (!endpoint.has_value()) {
        completeCurrent(OperationStatus::Rejected, QStringLiteral("unknown-device"));
        return;
    }
    operation.address = endpoint->address;
    operation.attempt = 1;
    operation.deadline =
        m_clock->monotonicMilliseconds() + static_cast<quint64>(m_requestTimeout);
    m_operationInFlight = true;
    if (!transmit(operation.mac, operation.datagram)) {
        completeCurrent(OperationStatus::Failed, QStringLiteral("send-failed"),
                        QStringLiteral("The light could not be reached."));
    }
}

void WizClient::completeCurrent(const OperationStatus status, const QString &reasonCode,
                                const QString &diagnostic)
{
    if (m_queue.isEmpty()) {
        return;
    }
    const PendingOperation operation = m_queue.takeFirst();
    m_operationInFlight = false;

    OperationResult result;
    result.kind = operation.kind;
    result.status = status;
    result.targetMac = operation.mac;
    result.initiatingEpoch = operation.initiatingEpoch;
    result.initiatingRevision = operation.initiatingRevision;
    result.observedEpoch = m_model.epoch();
    result.observedRevision = m_model.revision();
    result.reasonCode = reasonCode;
    result.diagnostic = diagnostic;
    deliver(result, operation.requestId);

    beginNextOperation();
}

} // namespace QindaQt::Wiz
