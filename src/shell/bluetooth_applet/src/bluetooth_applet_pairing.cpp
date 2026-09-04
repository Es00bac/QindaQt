// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_applet_controller.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <algorithm>

namespace QindaQt::Shell::BluetoothApplet
{

bool BluetoothAppletController::pairingPromptVisible() const noexcept
{
    return presentationOwnerAvailable()
        && m_client->snapshot().pairingPrompt.active();
}

bool BluetoothAppletController::pairingConfirmationAvailable() const noexcept
{
    if (!pairingPromptVisible()) {
        return false;
    }
    const Bluetooth::PairingPromptKind kind =
        m_client->snapshot().pairingPrompt.kind;
    return kind == Bluetooth::PairingPromptKind::ConfirmPasskey
        || kind == Bluetooth::PairingPromptKind::AuthorizeService;
}

QString BluetoothAppletController::pairingPromptText() const
{
    if (!pairingPromptVisible()) {
        return {};
    }
    const Bluetooth::Snapshot snapshot = m_client->snapshot();
    const Bluetooth::PairingPrompt &prompt = snapshot.pairingPrompt;
    const QString rowId = deviceRowId(prompt.device);
    const auto row = std::ranges::find_if(
        m_model.devices, [&rowId](const DeviceRow &candidate) {
            return candidate.id == rowId;
        });
    const QString label = row == m_model.devices.cend()
        ? tr("Bluetooth device") : row->label;
    switch (prompt.kind) {
    case Bluetooth::PairingPromptKind::ConfirmPasskey:
        return tr("Confirm passkey %1 for %2.").arg(prompt.detail, label);
    case Bluetooth::PairingPromptKind::EnterPasskey:
        return tr("%1 needs a passkey. Open Bluetooth Settings to enter it.").arg(label);
    case Bluetooth::PairingPromptKind::EnterPin:
        return tr("%1 needs a PIN. Open Bluetooth Settings to enter it.").arg(label);
    case Bluetooth::PairingPromptKind::DisplayPasskey:
        return tr("Type passkey %1 on %2 (%3 of 6 digits entered).")
            .arg(prompt.detail, label).arg(prompt.entered);
    case Bluetooth::PairingPromptKind::DisplayPin:
        return tr("Type PIN %1 on %2.").arg(prompt.detail, label);
    case Bluetooth::PairingPromptKind::AuthorizeService:
        return tr("Allow %1 to use Bluetooth service %2?")
            .arg(label, prompt.serviceUuid);
    case Bluetooth::PairingPromptKind::None:
        return {};
    }
    return {};
}

bool BluetoothAppletController::confirmPrompt()
{
    if (!pairingConfirmationAvailable() || pairingReplyPending()
        || !m_bluetoothControlGranted) {
        publishFeedback(tr("That pairing request cannot be confirmed here."));
        return false;
    }
    m_promptRequestId = m_client->replyConfirmation(true);
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

bool BluetoothAppletController::cancelPrompt()
{
    if (!pairingPromptVisible() || pairingReplyPending()
        || !m_bluetoothControlGranted) {
        publishFeedback(tr("That pairing request cannot be canceled here."));
        return false;
    }
    m_promptRequestId = pairingConfirmationAvailable()
        ? m_client->replyConfirmation(false) : m_client->cancelPrompt();
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

void BluetoothAppletController::handlePromptCompleted(
    const Bluetooth::OperationResult &result)
{
    const bool exact = m_client->owner() == m_promptOwner && result.wireValid
        && Bluetooth::validateOperationResult(result).accepted
        && result.initiatingEpoch == m_promptEpoch;
    m_promptRequestId = 0;
    m_promptOwner.clear();
    m_promptEpoch = 0;
    if (!exact || result.status == Bluetooth::OperationStatus::Uncertain) {
        publishFeedback(tr("The pairing response is uncertain. Check the current prompt."));
    } else if (result.status != Bluetooth::OperationStatus::Succeeded) {
        publishFeedback(tr("The pairing response was rejected (%1).")
                            .arg(result.reasonCode));
    } else {
        publishFeedback({});
    }
    reproject();
}

} // namespace QindaQt::Shell::BluetoothApplet
