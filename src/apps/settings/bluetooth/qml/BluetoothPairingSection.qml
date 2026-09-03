// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var bluetoothSettings
    readonly property var prompt: bluetoothSettings.pairingPrompt
    readonly property bool promptActive: prompt.active === true
    Layout.fillWidth: true
    spacing: Tokens.space["2"]
    visible: promptActive

    function cancelActivePrompt() {
        if (prompt.confirmationAvailable === true)
            bluetoothSettings.replyConfirmation(false)
        else
            bluetoothSettings.cancelPrompt()
    }

    Shortcut {
        // AGENT-GUARD: sequence needs a QKeySequence string; Qt.Key_Escape is
        // an integer key code and silently fails in the offscreen Quick path.
        sequence: "Escape"
        context: Qt.WindowShortcut
        enabled: root.promptActive
                 && !root.bluetoothSettings.pairingReplyPending
        onActivated: root.cancelActivePrompt()
    }

    function promptMessage() {
        if (prompt.kind === "confirm-passkey")
            return qsTr("Confirm that %1 shows passkey %2.")
                .arg(prompt.deviceLabel).arg(prompt.detail)
        if (prompt.kind === "authorize-service")
            return qsTr("Allow %1 to use Bluetooth service %2?")
                .arg(prompt.deviceLabel).arg(prompt.serviceUuid)
        if (prompt.kind === "enter-passkey")
            return qsTr("Enter the six-digit passkey for %1.")
                .arg(prompt.deviceLabel)
        if (prompt.kind === "enter-pin")
            return qsTr("Enter the PIN for %1.").arg(prompt.deviceLabel)
        if (prompt.kind === "display-passkey")
            return qsTr("Type passkey %1 on %2 (%3 of 6 digits entered).")
                .arg(prompt.detail).arg(prompt.deviceLabel).arg(prompt.entered)
        return qsTr("Type PIN %1 on %2.")
            .arg(prompt.detail).arg(prompt.deviceLabel)
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Pairing request")
        description: root.prompt.deviceLabel ?? qsTr("Bluetooth device")
    }

    FormSurface {
        Layout.fillWidth: true
        padding: Tokens.space["3"]
        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Pairing request for %1")
            .arg(root.prompt.deviceLabel ?? qsTr("Bluetooth device"))

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            Label {
                objectName: "bluetoothPairingMessage"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: root.promptMessage()
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            TextField {
                id: pairingInput
                objectName: "bluetoothPairingInput"
                Layout.fillWidth: true
                visible: root.prompt.passkeyInput === true
                         || root.prompt.pinInput === true
                enabled: !root.bluetoothSettings.pairingReplyPending
                maximumLength: root.prompt.passkeyInput === true ? 6 : 16
                validator: root.prompt.passkeyInput === true
                           ? passkeyValidator : pinValidator
                inputMethodHints: root.prompt.passkeyInput === true
                                  ? Qt.ImhDigitsOnly : Qt.ImhNoPredictiveText
                accessibleName: root.prompt.passkeyInput === true
                                ? qsTr("Bluetooth passkey")
                                : qsTr("Bluetooth PIN")
                accessibleDescription: qsTr("Required pairing response for %1")
                    .arg(root.prompt.deviceLabel ?? qsTr("Bluetooth device"))
                onAccepted: submitButton.clicked()
            }

            RegularExpressionValidator {
                id: passkeyValidator
                regularExpression: /^[0-9]{1,6}$/
            }

            RegularExpressionValidator {
                id: pinValidator
                regularExpression: /^[A-Za-z0-9]{1,16}$/
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    id: submitButton
                    objectName: "bluetoothPairingConfirm"
                    visible: root.prompt.confirmationAvailable === true
                             || pairingInput.visible
                    available: root.prompt.confirmationAvailable === true
                               || pairingInput.acceptableInput
                    busy: root.bluetoothSettings.pairingReplyPending
                    text: root.prompt.confirmationAvailable === true
                          ? qsTr("Confirm") : qsTr("Submit")
                    accessibleDescription: qsTr("Accept the pairing request for %1")
                        .arg(root.prompt.deviceLabel ?? qsTr("Bluetooth device"))
                    onClicked: {
                        if (root.prompt.confirmationAvailable === true)
                            root.bluetoothSettings.replyConfirmation(true)
                        else if (root.prompt.passkeyInput === true)
                            root.bluetoothSettings.replyPasskey(pairingInput.text)
                        else
                            root.bluetoothSettings.replyPin(pairingInput.text)
                    }
                }

                Button {
                    objectName: "bluetoothPairingCancel"
                    Layout.fillWidth: true
                    available: true
                    busy: root.bluetoothSettings.pairingReplyPending
                    emphasized: false
                    text: qsTr("Cancel")
                    accessibleDescription: qsTr("Cancel the pairing request for %1")
                        .arg(root.prompt.deviceLabel ?? qsTr("Bluetooth device"))
                    onClicked: root.cancelActivePrompt()
                }
            }
        }
    }
}
