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

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            Layout.fillWidth: true
            text: root.row.label
            color: root.colors.text ?? "white"
            elide: Text.ElideRight
            Accessible.name: root.row.accessibleName
            Accessible.description: root.row.accessibleDescription
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                objectName: "bluetoothAppletPowerButton"
                text: root.row.powered ? qsTr("Turn off") : qsTr("Turn on")
                enabled: root.row.canSetPowered
                focusPolicy: Qt.TabFocus
                Accessible.name: qsTr("%1: %2").arg(root.row.label).arg(text)
                Accessible.description: root.row.accessibleDescription
                onClicked: root.access.requestAdapterPower(root.row.id,
                                                           !root.row.powered)
            }

            Button {
                objectName: "bluetoothAppletDiscoveryButton"
                visible: root.row.powered
                text: root.row.canReleaseDiscovery
                      ? qsTr("Stop discovery") : qsTr("Discover devices")
                enabled: root.row.canAcquireDiscovery
                         || root.row.canReleaseDiscovery
                focusPolicy: Qt.TabFocus
                Accessible.name: qsTr("%1: %2").arg(root.row.label).arg(text)
                Accessible.description: root.row.accessibleDescription
                onClicked: root.access.requestDiscovery(
                    root.row.id, !root.row.canReleaseDiscovery)
            }
        }
    }
}
