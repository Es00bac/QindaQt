// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluez_adapter_backend_p.h"

namespace QindaQt::Bluetooth
{

void BluezAdapterBackend::submitPair(const quint64 operationId,
                                     const BackendRequest &request)
{
    const auto *device = d->store.deviceByAddress(request.deviceAddress);
    const auto *adapter = d->store.adapterByAddress(request.adapterAddress);
    if (device == nullptr || adapter == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    if (device->paired) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("already-paired"));
        return;
    }
    if (!adapter->powered) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("adapter-off"));
        return;
    }
    const quint64 callId = d->transport.pairDevice(device->path);
    if (callId == 0) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    insertDeviceCall(callId, operationId, request, OperationKind::Pair);
}

void BluezAdapterBackend::submitCancelPairing(const quint64 operationId,
                                              const BackendRequest &request)
{
    const auto *device = d->store.deviceByAddress(request.deviceAddress);
    if (device == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    (void)d->pairingAgent.cancelPrompt();
    const quint64 callId = d->transport.cancelPairing(device->path);
    if (callId == 0) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    insertDeviceCall(callId, operationId, request, OperationKind::CancelPairing);
}

void BluezAdapterBackend::submitRemove(const quint64 operationId,
                                       const BackendRequest &request)
{
    const auto *device = d->store.deviceByAddress(request.deviceAddress);
    const auto *adapter = d->store.adapterByAddress(request.adapterAddress);
    if (device == nullptr || adapter == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    const quint64 callId = d->transport.removeDevice(adapter->path, device->path);
    if (callId == 0) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    insertDeviceCall(callId, operationId, request, OperationKind::RemoveDevice);
}

void BluezAdapterBackend::submitSetTrusted(const quint64 operationId,
                                           const BackendRequest &request)
{
    const auto *device = d->store.deviceByAddress(request.deviceAddress);
    if (device == nullptr) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-handle"));
        return;
    }
    const quint64 callId =
        d->transport.setDeviceTrusted(device->path, request.trusted);
    if (callId == 0) {
        finishOperation(operationId, BackendOperationStatus::Uncertain,
                        QStringLiteral("bluez-transport"));
        return;
    }
    d->outstanding.insert(callId,
                          {.operationId = operationId,
                           .kind = OperationKind::SetTrusted,
                           .callerId = request.callerId,
                           .adapterAddress = request.adapterAddress,
                           .deviceAddress = request.deviceAddress,
                           .powered = request.trusted});
}

void BluezAdapterBackend::submitPromptReply(const quint64 operationId,
                                            const BackendRequest &request)
{
    if (!d->pairingPrompt.deviceAddress.isEmpty()
        && d->pairingPrompt.deviceAddress != request.deviceAddress) {
        finishOperation(operationId, BackendOperationStatus::Rejected,
                        QStringLiteral("stale-prompt"));
        return;
    }
    const PairingPromptKind promptKind = d->pairingPrompt.kind;
    bool accepted = false;
    switch (request.kind) {
    case OperationKind::ReplyConfirmation:
        accepted = d->pairingAgent.replyConfirmation(request.accepted);
        break;
    case OperationKind::ReplyPasskey:
        accepted = d->pairingAgent.replyPasskey(
            pairingInputString(request.input, request.inputSize));
        break;
    case OperationKind::ReplyPin:
        accepted = d->pairingAgent.replyPin(
            pairingInputString(request.input, request.inputSize));
        break;
    case OperationKind::CancelPrompt:
        accepted = d->pairingAgent.cancelPrompt();
        if (accepted && (promptKind == PairingPromptKind::DisplayPasskey
                         || promptKind == PairingPromptKind::DisplayPin)) {
            const auto *device = d->store.deviceByAddress(request.deviceAddress);
            const quint64 callId = device == nullptr
                ? 0 : d->transport.cancelPairing(device->path);
            if (callId == 0) {
                finishOperation(operationId, BackendOperationStatus::Uncertain,
                                QStringLiteral("bluez-transport"));
                return;
            }
            insertDeviceCall(callId, operationId, request,
                             OperationKind::CancelPrompt);
            return;
        }
        break;
    default:
        break;
    }
    finishOperation(operationId,
                    accepted ? BackendOperationStatus::Succeeded
                             : BackendOperationStatus::Rejected,
                    accepted ? QStringLiteral("prompt-replied")
                             : QStringLiteral("no-prompt"));
}

void BluezAdapterBackend::finishPairingCallSuccess(
    const quint64 operationId, const OperationKind kind,
    const QString &deviceAddress, const bool trusted)
{
    QString reasonCode;
    if (kind == OperationKind::Pair) {
        if (const auto *device = d->store.deviceByAddress(deviceAddress);
            device != nullptr) {
            d->store.applyPaired(device->path, true);
        }
        publish();
        reasonCode = QStringLiteral("paired");
    } else if (kind == OperationKind::RemoveDevice) {
        if (const auto *device = d->store.deviceByAddress(deviceAddress);
            device != nullptr) {
            d->store.removeInterfaces(device->path,
                                      {QStringLiteral("org.bluez.Device1")});
        }
        publish();
        reasonCode = QStringLiteral("device-removed");
    } else if (kind == OperationKind::SetTrusted) {
        if (const auto *device = d->store.deviceByAddress(deviceAddress);
            device != nullptr) {
            d->store.applyTrusted(device->path, trusted);
        }
        publish();
        reasonCode = QStringLiteral("trusted-set");
    } else {
        reasonCode = kind == OperationKind::CancelPrompt
            ? QStringLiteral("prompt-canceled")
            : QStringLiteral("pairing-canceled");
    }
    finishOperation(operationId, BackendOperationStatus::Succeeded, reasonCode);
}

} // namespace QindaQt::Bluetooth
