// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>

namespace QindaQt::Bluetooth
{

quint64 BluetoothClient::pairDevice(const Handle &device)
{
    return beginOperation({.kind = OperationKind::Pair, .target = device});
}

quint64 BluetoothClient::cancelPairing(const Handle &device)
{
    return beginOperation({.kind = OperationKind::CancelPairing, .target = device});
}

quint64 BluetoothClient::removeDevice(const Handle &device)
{
    return beginOperation({.kind = OperationKind::RemoveDevice, .target = device});
}

quint64 BluetoothClient::setTrusted(const Handle &device, const bool trusted)
{
    return beginOperation({.kind = OperationKind::SetTrusted,
                           .target = device,
                           .trusted = trusted});
}

quint64 BluetoothClient::replyConfirmation(const bool accepted)
{
    const Handle target = m_snapshot.has_value() ? m_snapshot->pairingPrompt.device
                                                  : Handle{};
    const quint64 promptId = m_snapshot.has_value()
        ? m_snapshot->pairingPrompt.promptId : 0;
    return beginOperation({.kind = OperationKind::ReplyConfirmation,
                           .target = target,
                           .accepted = accepted,
                           .promptId = promptId});
}

quint64 BluetoothClient::replyPasskey(const QString &passkey)
{
    const Handle target = m_snapshot.has_value() ? m_snapshot->pairingPrompt.device
                                                  : Handle{};
    const quint64 promptId = m_snapshot.has_value()
        ? m_snapshot->pairingPrompt.promptId : 0;
    OperationRequest request{.kind = OperationKind::ReplyPasskey,
                             .target = target,
                             .promptId = promptId};
    (void)setPairingInput(request.input, request.inputSize, passkey);
    return beginOperation(request);
}

quint64 BluetoothClient::replyPin(const QString &pin)
{
    const Handle target = m_snapshot.has_value() ? m_snapshot->pairingPrompt.device
                                                  : Handle{};
    const quint64 promptId = m_snapshot.has_value()
        ? m_snapshot->pairingPrompt.promptId : 0;
    OperationRequest request{.kind = OperationKind::ReplyPin,
                             .target = target,
                             .promptId = promptId};
    (void)setPairingInput(request.input, request.inputSize, pin);
    return beginOperation(request);
}

quint64 BluetoothClient::cancelPrompt()
{
    const Handle target = m_snapshot.has_value() ? m_snapshot->pairingPrompt.device
                                                  : Handle{};
    const quint64 promptId = m_snapshot.has_value()
        ? m_snapshot->pairingPrompt.promptId : 0;
    return beginOperation({.kind = OperationKind::CancelPrompt,
                           .target = target,
                           .promptId = promptId});
}

} // namespace QindaQt::Bluetooth
