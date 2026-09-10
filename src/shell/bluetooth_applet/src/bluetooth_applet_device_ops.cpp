// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_applet_controller.h"

#include <optional>

namespace QindaQt::Shell::BluetoothApplet
{

bool BluetoothAppletController::requestPairing(const QString &deviceId)
{
    const std::optional<Bluetooth::Device> device = findDevice(deviceId);
    if (!device.has_value()) {
        publishFeedback(tr("That Bluetooth device is no longer listed."));
        return false;
    }
    const Bluetooth::OperationRequest operation{
        .kind = Bluetooth::OperationKind::Pair,
        .target = device->handle,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    return dispatch(request);
}

bool BluetoothAppletController::requestPairingCancel()
{
    // AGENT-GUARD: The client serializes CancelPairing on its prompt-reply
    // lane, not the ordinary lane, so this path fences pairingReplyPending()
    // and must not route through dispatch(). Only this applet's own in-flight
    // Pair is a valid target; never synthesize a cancel for a foreign or
    // already-finished operation.
    if (!requestInFlight()
        || m_request.operation.kind != Bluetooth::OperationKind::Pair) {
        publishFeedback(tr("This applet has no pairing request to cancel."));
        return false;
    }
    if (pairingReplyPending()) {
        publishFeedback(tr("A pairing response is already in progress."));
        return false;
    }
    const Bluetooth::OperationRequest operation{
        .kind = Bluetooth::OperationKind::CancelPairing,
        .target = m_request.operation.target,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    if (!request.pending()) {
        publishFeedback(request.feedback);
        return false;
    }
    m_promptRequestId = m_client->cancelPairing(operation.target);
    if (m_promptRequestId == 0) {
        publishFeedback(tr("The pairing response could not be sent."));
        return false;
    }
    m_promptOwner = m_client->owner();
    m_promptEpoch = m_client->snapshot().epoch;
    publishFeedback({});
    Q_EMIT stateChanged();
    return true;
}

bool BluetoothAppletController::requestRemoval(const QString &deviceId)
{
    const std::optional<Bluetooth::Device> device = findDevice(deviceId);
    if (!device.has_value()) {
        publishFeedback(tr("That Bluetooth device is no longer listed."));
        return false;
    }
    const Bluetooth::OperationRequest operation{
        .kind = Bluetooth::OperationKind::RemoveDevice,
        .target = device->handle,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    return dispatch(request);
}

bool BluetoothAppletController::requestTrusted(const QString &deviceId,
                                                const bool trusted)
{
    const std::optional<Bluetooth::Device> device = findDevice(deviceId);
    if (!device.has_value()) {
        publishFeedback(tr("That Bluetooth device is no longer listed."));
        return false;
    }
    const Bluetooth::OperationRequest operation{
        .kind = Bluetooth::OperationKind::SetTrusted,
        .target = device->handle,
        .trusted = trusted,
    };
    const RequestState request = beginBluetoothRequest(
        m_client->snapshot(), operation, m_bluetoothControlGranted,
        m_discoveryLease);
    return dispatch(request);
}

} // namespace QindaQt::Shell::BluetoothApplet
