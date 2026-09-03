// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>

#include <QtCore/QMetaObject>

#include <utility>

namespace QindaQt::Bluetooth
{

void BluetoothClient::queueOperationCompletion(const quint64 requestId,
                                               OperationResult result)
{
    Q_ASSERT(requestId != 0);
    Q_ASSERT(!m_queuedOperationCompletions.contains(requestId));
    if (requestId == 0 || m_queuedOperationCompletions.contains(requestId)) {
        return;
    }

    m_queuedOperationCompletions.insert(requestId, std::move(result));
    // AGENT-GUARD: Never replace this with a direct emit. Callers must receive
    // the request ID before same-thread handlers can observe completion. Using
    // this object as the invocation context also cancels delivery on QObject
    // destruction; stop() removes entries whose lifetime it cancels.
    const bool queued = QMetaObject::invokeMethod(
        this,
        [this, requestId] {
            const auto it = m_queuedOperationCompletions.find(requestId);
            if (it == m_queuedOperationCompletions.end()) {
                return;
            }
            const OperationResult completion = it.value();
            m_queuedOperationCompletions.erase(it);
            Q_EMIT operationCompleted(requestId, completion);
        },
        Qt::QueuedConnection);
    if (!queued) {
        m_queuedOperationCompletions.remove(requestId);
    }
}

void BluetoothClient::cancelQueuedOperationCompletions()
{
    m_queuedOperationCompletions.clear();
}

void BluetoothClient::completeUncertain(const QString &reasonCode)
{
    const auto complete = [this, &reasonCode](std::optional<PendingOperation> &lane,
                                              QTimer &timer) {
        if (!lane.has_value()) return;
        const PendingOperation pending = *lane;
        lane.reset();
        timer.stop();
        queueOperationCompletion(
            pending.requestId,
            {.kind = pending.request.kind,
             .status = OperationStatus::Uncertain,
             .initiatingEpoch = pending.epoch,
             .initiatingRevision = pending.revision,
             .observedEpoch = m_snapshot.has_value() ? m_snapshot->epoch
                                                      : pending.epoch,
             .observedRevision = m_snapshot.has_value() ? m_snapshot->revision
                                                         : pending.revision,
             .reasonCode = reasonCode,
             .diagnostic = {},
             .wireValid = true});
    };
    complete(m_operation, m_operationTimer);
    complete(m_promptOperation, m_promptOperationTimer);
}

} // namespace QindaQt::Bluetooth
