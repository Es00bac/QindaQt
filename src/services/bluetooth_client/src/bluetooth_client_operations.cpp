// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <limits>

namespace QindaQt::Bluetooth
{

// Module-private operation preflight implemented with the client lifecycle
// helpers; it is intentionally absent from the installed public header.
QString preflightOperation(const Snapshot &snapshot,
                           const OperationRequest &request);

quint64 BluetoothClient::beginOperation(const OperationRequest &request)
{
    if (m_nextRequestId == 0
        || m_nextRequestId == std::numeric_limits<quint64>::max()) return 0;
    const quint64 requestId = m_nextRequestId++;
    const bool promptLane = request.kind == OperationKind::ReplyConfirmation
        || request.kind == OperationKind::ReplyPasskey
        || request.kind == OperationKind::ReplyPin
        || request.kind == OperationKind::CancelPrompt
        || request.kind == OperationKind::CancelPairing;
    std::optional<PendingOperation> &lane = promptLane
        ? m_promptOperation : m_operation;
    if (lane.has_value()) {
        queueOperationCompletion(
            requestId,
            localResult(request, OperationStatus::Busy,
                        QStringLiteral("operation-busy")));
        return requestId;
    }
    if (m_owner.isEmpty() || !m_snapshot.has_value()) {
        queueOperationCompletion(
            requestId,
            localResult(request, OperationStatus::Rejected,
                        QStringLiteral("unavailable")));
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
    }
    lane = PendingOperation{.requestId = requestId,
                            .request = request,
                            .epoch = m_snapshot->epoch,
                            .revision = m_snapshot->revision};
    (promptLane ? m_promptOperationTimer : m_operationTimer)
        .start(m_requestTimeoutMs);
    m_transport->submitOperation(m_owner, requestId, request);
    return requestId;
}

quint64 BluetoothClient::setAdapterPower(const Handle &adapter,
                                         const bool powered)
{
    return beginOperation({.kind = OperationKind::SetAdapterPower,
                           .target = adapter,
                           .powered = powered});
}

quint64 BluetoothClient::acquireDiscovery(const Handle &adapter)
{
    return beginOperation({.kind = OperationKind::AcquireDiscovery,
                           .target = adapter});
}

quint64 BluetoothClient::releaseDiscovery(const Handle &adapter)
{
    return beginOperation({.kind = OperationKind::ReleaseDiscovery,
                           .target = adapter});
}

quint64 BluetoothClient::connectDevice(const Handle &device)
{
    return beginOperation({.kind = OperationKind::Connect, .target = device});
}

quint64 BluetoothClient::disconnectDevice(const Handle &device)
{
    return beginOperation({.kind = OperationKind::Disconnect, .target = device});
}

} // namespace QindaQt::Bluetooth
