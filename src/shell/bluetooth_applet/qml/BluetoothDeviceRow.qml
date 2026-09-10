// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

Frame {
    id: root

    required property var row
    required property var access
    required property var colors

    RowLayout {
        anchors.fill: parent
        spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.row.label
                color: root.colors.text ?? "white"
                elide: Text.ElideRight
                Accessible.name: root.row.accessibleName
                Accessible.description: root.row.accessibleDescription
            }

            Label {
                Layout.fillWidth: true
                text: root.row.accessibleDescription
                color: root.colors.textMuted ?? "#a9afa9"
                wrapMode: Text.Wrap
            }
        }

        C.Switch {
            objectName: "bluetoothAppletTrustSwitch"
            visible: root.row.paired
            text: qsTr("Trust")
            enabled: root.row.canSetTrusted
            checked: root.row.trusted
            accessibleDescription: qsTr("Allow %1 to connect without asking")
                .arg(root.row.label)
            onToggled: root.access.requestTrusted(root.row.id, checked)
        }

        C.Button {
            objectName: "bluetoothAppletPairButton"
            visible: !root.row.paired
            text: root.row.pending ? qsTr("Cancel pairing") : qsTr("Pair")
            // A pending Pair can only be canceled while the prompt-reply
            // lane is free; the controller fences the same condition.
            available: root.row.pending
                       ? (root.access !== null && !root.access.pairingReplyPending)
                       : root.row.canPair
            accessibleDescription: root.row.pending
                ? qsTr("Cancel the in-progress pairing with %1").arg(root.row.label)
                : qsTr("Pair with %1").arg(root.row.label)
            onClicked: {
                if (root.row.pending)
                    root.access.requestPairingCancel()
                else
                    root.access.requestPairing(root.row.id)
            }
        }

        Button {
            objectName: "bluetoothAppletConnectionButton"
            visible: root.row.paired
            text: root.row.connected ? qsTr("Disconnect") : qsTr("Connect")
            enabled: root.row.connected ? root.row.canDisconnect
                                        : root.row.canConnect
            focusPolicy: Qt.TabFocus
            Accessible.name: qsTr("%1 %2").arg(text).arg(root.row.label)
            Accessible.description: root.row.accessibleDescription
            onClicked: root.access.requestDeviceConnection(root.row.id,
                                                            !root.row.connected)
        }

        C.Button {
            objectName: "bluetoothAppletForgetButton"
            visible: root.row.paired
            emphasized: false
            destructive: true
            text: qsTr("Forget")
            available: root.row.canRemove
            accessibleDescription: qsTr("Forget %1 and remove its pairing")
                .arg(root.row.label)
            onClicked: root.access.requestRemoval(root.row.id)
        }
    }
}
