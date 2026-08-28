// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_service_object_p.h"

#include <QtDBus/QDBusConnection>

namespace QindaQt::Bluetooth
{

BluetoothServiceObject::BluetoothServiceObject(BluetoothModel *model,
                                               const QDBusConnection &connection,
                                               QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_connection(connection)
{
    Q_ASSERT(m_model != nullptr);
    connect(m_model, &BluetoothModel::invalidated, this, &BluetoothServiceObject::Changed);
    connect(m_model, &BluetoothModel::operationCompleted, this,
            &BluetoothServiceObject::finishOperation);
}

Snapshot BluetoothServiceObject::GetSnapshot() const
{
    return m_model->snapshot();
}

void BluetoothServiceObject::SetPowered(const Handle &adapter, const bool powered)
{
    beginOperation({.kind = OperationKind::SetAdapterPower,
                    .target = adapter,
                    .powered = powered});
}

void BluetoothServiceObject::AcquireDiscovery(const Handle &adapter)
{
    beginOperation(
        {.kind = OperationKind::AcquireDiscovery, .target = adapter, .powered = false});
}

void BluetoothServiceObject::ReleaseDiscovery(const Handle &adapter)
{
    beginOperation(
        {.kind = OperationKind::ReleaseDiscovery, .target = adapter, .powered = false});
}

void BluetoothServiceObject::Connect(const Handle &device)
{
    beginOperation({.kind = OperationKind::Connect, .target = device, .powered = false});
}

void BluetoothServiceObject::Disconnect(const Handle &device)
{
    beginOperation({.kind = OperationKind::Disconnect, .target = device, .powered = false});
}

void BluetoothServiceObject::beginOperation(const OperationRequest &request)
{
    if (!calledFromDBus()) {
        return;
    }

    const QDBusMessage call = message();
    setDelayedReply(true);
    // AGENT-GUARD: Discovery leases are caller-scoped, so every operation is
    // attributed to the exact unique bus name that requested it. A spoofed or
    // rebound well-known name cannot inherit another caller's lease.
    const QString caller = call.service();
    const OperationSubmission submission = m_model->submit(request, caller);
    if (!submission.pending) {
        m_connection.send(call.createReply(QVariant::fromValue(submission.immediateResult)));
        return;
    }

    // AGENT-GUARD: The model caps all pending operations before this
    // insertion. Keeping exactly one original call per operation prevents a
    // timeout or authority replacement from being accidentally replayed.
    m_pendingReplies.insert(submission.operationId, call);
}

void BluetoothServiceObject::finishOperation(const quint64 operationId,
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

} // namespace QindaQt::Bluetooth
