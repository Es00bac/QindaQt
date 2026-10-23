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

    padding: 6
    contentItem: RowLayout {
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
                text: root.row.connected ? qsTr("Connected") : (root.row.paired ? qsTr("Paired") : qsTr("Available"))
                color: root.colors.textMuted ?? "#a9afa9"
                elide: Text.ElideRight
            }
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

        C.Button {
            emphasized: false
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

        ToolButton {
            objectName: "bluetoothAppletDeviceMenuButton"
            visible: root.row.paired
            implicitWidth: 28
            contentItem: Label {
                text: "⋮"
                color: root.colors.text ?? "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Accessible.name: qsTr("More options for %1").arg(root.row.label)
            onClicked: {
                deviceMenu.deviceId = root.row.id
                deviceMenu.popup()
            }
            Menu {
                id: deviceMenu
                property string deviceId: ""
                MenuItem {
                    objectName: "bluetoothAppletTrustSwitch"
                    text: root.row.trusted ? qsTr("Stop trusting") : qsTr("Trust device")
                    enabled: root.row.canSetTrusted && deviceMenu.deviceId === root.row.id
                    onTriggered: root.access.requestTrusted(deviceMenu.deviceId, !root.row.trusted)
                }
                MenuItem {
                    objectName: "bluetoothAppletForgetButton"
                    text: qsTr("Forget device")
                    enabled: root.row.canRemove && deviceMenu.deviceId === root.row.id
                    onTriggered: root.access.requestRemoval(deviceMenu.deviceId)
                }
            }
        }
    }
}
