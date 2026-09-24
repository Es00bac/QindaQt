// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

// One current connection with its Disconnect action.
Frame {
    id: root

    required property var row
    required property var access
    required property var colors

    objectName: "networkAppletConnectionRow"

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
                text: root.row.pending ? qsTr("%1 · disconnecting…").arg(root.row.kindLabel)
                                       : root.row.kindLabel
                color: root.colors.textMuted ?? "#a9afa9"
                elide: Text.ElideRight
            }
        }

        C.Button {
            objectName: "networkAppletDisconnectButton"
            text: qsTr("Disconnect")
            emphasized: false
            available: root.row.canDisconnect && !root.row.pending
            busy: root.row.pending
            accessibleDescription: qsTr("Disconnect %1").arg(root.row.label)
            onClicked: root.access.requestDisconnect(root.row.id)
        }
    }
}
