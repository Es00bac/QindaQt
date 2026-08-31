// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
                text: root.row.paired
                      ? root.row.accessibleDescription
                      : qsTr("Not paired; pairing is not available in this applet")
                color: root.colors.textMuted ?? "#a9afa9"
                wrapMode: Text.Wrap
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
    }
}
