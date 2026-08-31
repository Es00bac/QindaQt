// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var networkSettings
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Network devices")
        description: qsTr("Authoritative device and active-connection state from Network1")
    }

    Repeater {
        id: deviceRepeater
        model: root.networkSettings.devices

        delegate: FormSurface {
            id: deviceRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1 device %2, %3")
                .arg(deviceRow.modelData.kindText)
                .arg(deviceRow.modelData.interfaceName)
                .arg(deviceRow.modelData.stateText)

            contentItem: RowLayout {
                spacing: Tokens.space["3"]

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 — %2").arg(deviceRow.modelData.kindText)
                                              .arg(deviceRow.modelData.interfaceName)
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: deviceRow.modelData.activeNetworkName.length > 0
                              ? qsTr("%1 · %2").arg(deviceRow.modelData.stateText)
                                               .arg(deviceRow.modelData.activeNetworkName)
                              : deviceRow.modelData.stateText
                        muted: true
                    }
                }

                Button {
                    objectName: "networkDisconnect_" + deviceRow.modelData.interfaceName
                    visible: deviceRow.modelData.active
                    available: deviceRow.modelData.disconnectAvailable
                    busy: root.networkSettings.busy
                    emphasized: false
                    text: qsTr("Disconnect")
                    accessibleDescription: qsTr("Disconnect device %1")
                        .arg(deviceRow.modelData.interfaceName)
                    onClicked: root.networkSettings.disconnectDevice(
                                   deviceRow.modelData.interfaceName)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: deviceRepeater.count === 0
        text: qsTr("No network devices are currently reported.")
        muted: true
    }
}
