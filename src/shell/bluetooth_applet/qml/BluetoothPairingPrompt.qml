// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

Rectangle {
    id: root

    required property var access
    required property var colors
    required property var theme
    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + 16
    visible: access !== null && access.pairingPromptVisible
    radius: theme.cornerRadius ?? 10
    color: colors.surfaceRaised ?? "#2c312e"
    border.color: colors.warning ?? "#e5a84b"
    Accessible.role: Accessible.Pane
    Accessible.name: promptText.text

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Label {
            id: promptText
            objectName: "bluetoothAppletPairingText"
            Layout.fillWidth: true
            text: root.access !== null ? root.access.pairingPromptText : ""
            color: root.colors.text ?? "white"
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        C.TextField {
            id: pairingEntry
            objectName: "bluetoothAppletPairingEntry"
            readonly property bool passkeyEntry: root.access !== null
                && root.access.pairingPasskeyEntryAvailable
            Layout.fillWidth: true
            visible: root.access !== null
                     && (root.access.pairingPasskeyEntryAvailable
                         || root.access.pairingPinEntryAvailable)
            enabled: root.access !== null && !root.access.pairingReplyPending
            maximumLength: passkeyEntry ? 6 : 16
            validator: passkeyEntry ? passkeyValidator : pinValidator
            inputMethodHints: passkeyEntry ? Qt.ImhDigitsOnly
                                           : Qt.ImhNoPredictiveText
            accessibleName: passkeyEntry ? qsTr("Bluetooth passkey")
                                         : qsTr("Bluetooth PIN")
            accessibleDescription: qsTr("Required pairing response for the current Bluetooth request")
            onAccepted: {
                if (pairingEntry.acceptableInput)
                    submitEntry.clicked()
            }
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

            C.Button {
                objectName: "bluetoothAppletPairingConfirm"
                visible: root.access !== null
                         && root.access.pairingConfirmationAvailable
                available: root.access !== null
                           && !root.access.pairingReplyPending
                focusPolicy: Qt.StrongFocus
                text: qsTr("Confirm")
                accessibleDescription: qsTr("Confirm this Bluetooth pairing request")
                onClicked: root.access.confirmPrompt()
            }

            C.Button {
                id: submitEntry
                objectName: "bluetoothAppletPairingSubmit"
                visible: pairingEntry.visible
                available: root.access !== null
                           && !root.access.pairingReplyPending
                           && pairingEntry.acceptableInput
                focusPolicy: Qt.StrongFocus
                text: qsTr("Submit")
                accessibleDescription: qsTr("Submit this Bluetooth pairing response")
                onClicked: {
                    if (pairingEntry.passkeyEntry)
                        root.access.submitPasskey(pairingEntry.text)
                    else
                        root.access.submitPin(pairingEntry.text)
                    pairingEntry.clear()
                }
            }

            C.Button {
                objectName: "bluetoothAppletPairingCancel"
                Layout.fillWidth: true
                emphasized: false
                available: root.access !== null
                           && !root.access.pairingReplyPending
                focusPolicy: Qt.StrongFocus
                text: qsTr("Cancel")
                accessibleDescription: qsTr("Cancel this Bluetooth pairing request")
                onClicked: root.access.cancelPrompt()
            }
        }
    }
}
