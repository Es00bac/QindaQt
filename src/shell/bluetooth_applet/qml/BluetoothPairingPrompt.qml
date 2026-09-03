// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

        RowLayout {
            Layout.fillWidth: true

            Button {
                objectName: "bluetoothAppletPairingConfirm"
                visible: root.access !== null
                         && root.access.pairingConfirmationAvailable
                enabled: root.access !== null
                         && !root.access.pairingReplyPending
                focusPolicy: Qt.StrongFocus
                text: qsTr("Confirm")
                Accessible.name: text
                Accessible.description: qsTr("Confirm this Bluetooth pairing request")
                onClicked: root.access.confirmPrompt()
            }

            Button {
                objectName: "bluetoothAppletPairingCancel"
                Layout.fillWidth: true
                enabled: root.access !== null
                         && !root.access.pairingReplyPending
                focusPolicy: Qt.StrongFocus
                text: qsTr("Cancel")
                Accessible.name: text
                Accessible.description: qsTr("Cancel this Bluetooth pairing request")
                onClicked: root.access.cancelPrompt()
            }
        }
    }
}
