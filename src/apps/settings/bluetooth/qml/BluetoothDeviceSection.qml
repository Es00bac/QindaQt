// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var bluetoothSettings
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Devices")
        description: qsTr("Pair discovered devices and manage paired devices")
    }

    Repeater {
        id: deviceRepeater
        model: root.bluetoothSettings.devices

        delegate: FormSurface {
            id: deviceRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: deviceRow.modelData.label
            Accessible.description: deviceRow.modelData.accessibleDescription

            contentItem: GridLayout {
                objectName: "bluetoothDeviceLayout_" + deviceRow.modelData.id
                columns: deviceRow.width < 520 ? 1 : 3
                rowSpacing: Tokens.space["3"]
                columnSpacing: Tokens.space["3"]

                T.ToolButton {
                    objectName: "bluetoothClassIcon_" + deviceRow.modelData.id
                    readonly property string deviceIconName: deviceRow.modelData.iconName
                    icon.name: deviceIconName
                    display: T.AbstractButton.IconOnly
                    enabled: false
                    focusPolicy: Qt.NoFocus
                    background: null
                    Accessible.ignored: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Label {
                        Layout.fillWidth: true
                        text: deviceRow.modelData.label
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: deviceRow.modelData.classLabel + " · "
                              + (deviceRow.modelData.paired
                                 ? qsTr("Paired") : qsTr("Not paired"))
                              + " · "
                              + (deviceRow.modelData.connected
                                 ? qsTr("Connected") : qsTr("Disconnected"))
                        muted: true
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: deviceRow.modelData.rssiKnown
                        text: qsTr("Signal %1 dBm").arg(deviceRow.modelData.rssi)
                        muted: true
                        Accessible.name: text
                    }
                }

                Button {
                    objectName: "bluetoothPair_" + deviceRow.modelData.id
                    visible: !deviceRow.modelData.paired
                    available: deviceRow.modelData.pairAvailable
                    busy: root.bluetoothSettings.busy
                    Layout.fillWidth: deviceRow.width < 520
                    text: qsTr("Pair")
                    accessibleDescription: qsTr("Pair with Bluetooth device %1")
                        .arg(deviceRow.modelData.label)
                    onClicked: root.bluetoothSettings.requestPairing(
                                   deviceRow.modelData.id)
                }

                Button {
                    objectName: "bluetoothConnect_" + deviceRow.modelData.id
                    visible: deviceRow.modelData.paired
                             && !deviceRow.modelData.connected
                    available: deviceRow.modelData.connectAvailable
                    busy: root.bluetoothSettings.busy
                    Layout.fillWidth: deviceRow.width < 520
                    text: qsTr("Connect")
                    accessibleDescription: qsTr("Connect paired device %1")
                        .arg(deviceRow.modelData.label)
                    onClicked: root.bluetoothSettings.requestDeviceConnection(
                                   deviceRow.modelData.id, true)
                }

                Button {
                    objectName: "bluetoothTrust_" + deviceRow.modelData.id
                    visible: deviceRow.modelData.paired
                    available: deviceRow.modelData.trustAvailable
                    busy: root.bluetoothSettings.busy
                    Layout.fillWidth: deviceRow.width < 520
                    emphasized: false
                    text: deviceRow.modelData.trusted
                          ? qsTr("Untrust") : qsTr("Trust")
                    accessibleDescription: deviceRow.modelData.trusted
                        ? qsTr("Stop trusting Bluetooth device %1")
                              .arg(deviceRow.modelData.label)
                        : qsTr("Trust Bluetooth device %1")
                              .arg(deviceRow.modelData.label)
                    onClicked: root.bluetoothSettings.requestTrust(
                                   deviceRow.modelData.id,
                                   !deviceRow.modelData.trusted)
                }

                Button {
                    objectName: "bluetoothForget_" + deviceRow.modelData.id
                    visible: deviceRow.modelData.paired
                    available: deviceRow.modelData.forgetAvailable
                    busy: root.bluetoothSettings.busy
                    Layout.fillWidth: deviceRow.width < 520
                    emphasized: false
                    destructive: true
                    text: qsTr("Forget")
                    accessibleDescription: qsTr("Forget Bluetooth device %1")
                        .arg(deviceRow.modelData.label)
                    onClicked: root.bluetoothSettings.requestForget(
                                   deviceRow.modelData.id)
                }

                Button {
                    objectName: "bluetoothDisconnect_" + deviceRow.modelData.id
                    visible: deviceRow.modelData.connected
                    available: deviceRow.modelData.disconnectAvailable
                    busy: root.bluetoothSettings.busy
                    Layout.fillWidth: deviceRow.width < 520
                    emphasized: false
                    text: qsTr("Disconnect")
                    accessibleDescription: qsTr("Disconnect paired device %1")
                        .arg(deviceRow.modelData.label)
                    onClicked: root.bluetoothSettings.requestDeviceConnection(
                                   deviceRow.modelData.id, false)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: deviceRepeater.count === 0
        text: qsTr("No Bluetooth devices are currently reported.")
        muted: true
        Accessible.name: text
    }
}
