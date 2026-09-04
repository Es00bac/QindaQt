// SPDX-License-Identifier: LGPL-3.0-or-later

#include "bluez_adapter_backend_p.h"

namespace QindaQt::Bluetooth
{
namespace
{
using Bluez::BluezAdapterState;
using Bluez::BluezDeviceState;

struct MappedReply {
    BackendOperationStatus status = BackendOperationStatus::Failed;
    QString reasonCode;
};

MappedReply mapErrorReply(const QString &errorName)
{
    if (!errorName.startsWith(QLatin1String("org.bluez.Error.")))
        return {BackendOperationStatus::Uncertain, QStringLiteral("bluez-transport")};
    if (errorName == QLatin1String("org.bluez.Error.NotReady"))
        return {BackendOperationStatus::Rejected, QStringLiteral("adapter-off")};
    if (errorName == QLatin1String("org.bluez.Error.DoesNotExist")
        || errorName == QLatin1String("org.bluez.Error.UnknownObject"))
        return {BackendOperationStatus::Rejected, QStringLiteral("stale-handle")};
    if (errorName == QLatin1String("org.bluez.Error.AlreadyConnected"))
        return {BackendOperationStatus::Rejected, QStringLiteral("already-connected")};
    if (errorName == QLatin1String("org.bluez.Error.NotConnected"))
        return {BackendOperationStatus::Rejected, QStringLiteral("not-connected")};
    if (errorName == QLatin1String("org.bluez.Error.AuthenticationCanceled")
        || errorName == QLatin1String("org.bluez.Error.AuthenticationRejected")
        || errorName == QLatin1String("org.bluez.Error.AuthenticationFailed")
        || errorName == QLatin1String("org.bluez.Error.AuthenticationTimeout"))
        return {BackendOperationStatus::Rejected, QStringLiteral("pairing-rejected")};
    return {BackendOperationStatus::Failed, QStringLiteral("bluez-error")};
}
} // namespace

void BluezAdapterBackend::handleCallFinished(const quint64 callId,
                                             const bool succeeded,
                                             const QString &errorName)
{
    const auto it = d->outstanding.find(callId);
    if (it == d->outstanding.end()) return;
    const State::Outstanding call = it.value();
    d->outstanding.erase(it);
    switch (call.kind) {
    case OperationKind::SetAdapterPower: {
        if (!succeeded) {
            const MappedReply mapped = mapErrorReply(errorName);
            finishOperation(call.operationId, mapped.status, mapped.reasonCode);
            return;
        }
        if (const BluezAdapterState *adapter =
                d->store.adapterByAddress(call.adapterAddress);
            adapter != nullptr) {
            d->store.applyPowered(adapter->path, call.powered);
            if (!call.powered) d->dropAdapterLeases(call.adapterAddress);
        }
        publish();
        finishOperation(call.operationId, BackendOperationStatus::Succeeded,
                        QStringLiteral("adapter-power-set"));
        return;
    }
    case OperationKind::AcquireDiscovery: {
        const QList<State::Outstanding> queued =
            d->queuedAcquires.take(call.adapterAddress);
        d->inflightAcquires.remove(call.adapterAddress);
        const bool sessionActive = succeeded
            || errorName == QLatin1String("org.bluez.Error.AlreadyExists")
            || errorName == QLatin1String("org.bluez.Error.InProgress");
        if (sessionActive) {
            ++d->leases[{call.callerId, call.adapterAddress}];
            for (const State::Outstanding &waiter : queued)
                ++d->leases[{waiter.callerId, waiter.adapterAddress}];
            publish();
            finishOperation(call.operationId, BackendOperationStatus::Succeeded,
                            QStringLiteral("lease-acquired"));
            for (const State::Outstanding &waiter : queued)
                finishOperation(waiter.operationId, BackendOperationStatus::Succeeded,
                                QStringLiteral("lease-acquired"));
            return;
        }
        const MappedReply mapped = mapErrorReply(errorName);
        finishOperation(call.operationId, mapped.status, mapped.reasonCode);
        for (const State::Outstanding &waiter : queued)
            finishOperation(waiter.operationId, mapped.status, mapped.reasonCode);
        return;
    }
    case OperationKind::Connect:
    case OperationKind::Disconnect: {
        const bool connecting = call.kind == OperationKind::Connect;
        const QString idempotentError = connecting
            ? QStringLiteral("org.bluez.Error.AlreadyConnected")
            : QStringLiteral("org.bluez.Error.NotConnected");
        if (succeeded || errorName == idempotentError) {
            if (const BluezDeviceState *device =
                    d->store.deviceByAddress(call.deviceAddress);
                device != nullptr)
                d->store.applyConnected(device->path, connecting);
            publish();
            finishOperation(call.operationId,
                            succeeded ? BackendOperationStatus::Succeeded
                                      : BackendOperationStatus::Rejected,
                            succeeded
                                ? (connecting ? QStringLiteral("connected")
                                              : QStringLiteral("disconnected"))
                                : (connecting ? QStringLiteral("already-connected")
                                              : QStringLiteral("not-connected")));
            return;
        }
        const MappedReply mapped = mapErrorReply(errorName);
        finishOperation(call.operationId, mapped.status, mapped.reasonCode);
        return;
    }
    case OperationKind::Pair:
    case OperationKind::CancelPairing:
    case OperationKind::RemoveDevice:
    case OperationKind::SetTrusted:
    case OperationKind::CancelPrompt: {
        if (succeeded) {
            finishPairingCallSuccess(call.operationId, call.kind,
                                     call.deviceAddress, call.powered);
            return;
        }
        const MappedReply mapped = mapErrorReply(errorName);
        finishOperation(call.operationId, mapped.status, mapped.reasonCode);
        return;
    }
    case OperationKind::ReplyConfirmation:
    case OperationKind::ReplyPasskey:
    case OperationKind::ReplyPin:
    default:
        finishOperation(call.operationId, BackendOperationStatus::Failed,
                        QStringLiteral("backend-malformed"));
        return;
    }
}

} // namespace QindaQt::Bluetooth
