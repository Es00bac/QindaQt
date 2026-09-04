// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth1_service_object_p.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>

namespace QindaQt::Bluetooth
{

Bluetooth1ServiceObject::Bluetooth1ServiceObject(
    BluetoothModel *model, const QDBusConnection &connection, QObject *parent)
    : QObject(parent), m_model(model), m_connection(connection)
{
    Q_ASSERT(m_model != nullptr);
    connect(m_model, &BluetoothModel::invalidated, this,
            &Bluetooth1ServiceObject::Changed);
    connect(m_model, &BluetoothModel::operationCompleted, this,
            &Bluetooth1ServiceObject::finishOperation);
}

Bluetooth1Snapshot Bluetooth1ServiceObject::GetSnapshot() const
{
    return bluetooth1Projection(m_model->snapshot());
}

void Bluetooth1ServiceObject::SetPowered(const Handle &adapter,
                                         const bool powered)
{
    beginOperation({.kind = OperationKind::SetAdapterPower,
                    .target = adapter,
                    .powered = powered});
}

void Bluetooth1ServiceObject::AcquireDiscovery(const Handle &adapter)
{
    beginOperation({.kind = OperationKind::AcquireDiscovery, .target = adapter});
}

void Bluetooth1ServiceObject::ReleaseDiscovery(const Handle &adapter)
{
    beginOperation({.kind = OperationKind::ReleaseDiscovery, .target = adapter});
}

void Bluetooth1ServiceObject::Connect(const Handle &device)
{
    beginOperation({.kind = OperationKind::Connect, .target = device});
}

void Bluetooth1ServiceObject::Disconnect(const Handle &device)
{
    beginOperation({.kind = OperationKind::Disconnect, .target = device});
}

void Bluetooth1ServiceObject::beginOperation(const OperationRequest &request)
{
    if (!calledFromDBus()) {
        return;
    }
    const QDBusMessage call = message();
    setDelayedReply(true);
    const OperationSubmission submission = m_model->submit(request, call.service());
    if (!submission.pending) {
        m_connection.send(call.createReply(
            QVariant::fromValue(submission.immediateResult)));
        return;
    }
    m_pendingReplies.insert(submission.operationId, call);
}

void Bluetooth1ServiceObject::finishOperation(const quint64 operationId,
                                              const OperationResult &result)
{
    const auto it = m_pendingReplies.find(operationId);
    if (it == m_pendingReplies.end()) {
        return;
    }
    const QDBusMessage call = it.value();
    m_pendingReplies.erase(it);
    m_connection.send(call.createReply(QVariant::fromValue(result)));
}

} // namespace QindaQt::Bluetooth
