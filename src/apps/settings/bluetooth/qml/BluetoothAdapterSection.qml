// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
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
        title: qsTr("Adapters")
        description: qsTr("Turn Bluetooth on and search for nearby devices.")
    }

    Repeater {
        id: adapterRepeater
        model: root.bluetoothSettings.adapters

        delegate: FormSurface {
            id: adapterRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: adapterRow.modelData.label
            Accessible.description: adapterRow.modelData.accessibleDescription

            contentItem: GridLayout {
                objectName: "bluetoothAdapterLayout_" + adapterRow.modelData.id
                columns: adapterRow.width < 520 ? 1 : 3
                rowSpacing: Tokens.space["3"]
                columnSpacing: Tokens.space["3"]

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Label {
                        Layout.fillWidth: true
                        text: adapterRow.modelData.label
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: adapterRow.modelData.discovering
                              ? qsTr("Discovering devices")
                              : qsTr("Discovery stopped")
                        muted: true
                    }
                }

                Switch {
                    objectName: "bluetoothPower_" + adapterRow.modelData.id
                    text: qsTr("Power")
                    checked: adapterRow.modelData.powered
                    Layout.fillWidth: adapterRow.width < 520
                    enabled: adapterRow.modelData.powerAvailable
                    accessibleDescription: qsTr("Set power for %1")
                        .arg(adapterRow.modelData.label)
                    onClicked: root.bluetoothSettings.requestAdapterPower(
                                   adapterRow.modelData.id,
                                   !adapterRow.modelData.powered)
                }

                Button {
                    objectName: "bluetoothDiscovery_" + adapterRow.modelData.id
                    available: adapterRow.modelData.discoveryLeaseOwned
                               ? adapterRow.modelData.stopDiscoveryAvailable
                               : adapterRow.modelData.startDiscoveryAvailable
                    busy: root.bluetoothSettings.busy
                    emphasized: false
                    Layout.fillWidth: adapterRow.width < 520
                    text: adapterRow.modelData.discoveryLeaseOwned
                          ? qsTr("Stop discovery") : qsTr("Discover")
                    accessibleDescription: adapterRow.modelData.discoveryLeaseOwned
                        ? qsTr("Stop searching for devices using %1")
                              .arg(adapterRow.modelData.label)
                        : qsTr("Search for nearby devices using %1")
                              .arg(adapterRow.modelData.label)
                    onClicked: root.bluetoothSettings.requestDiscovery(
                                   adapterRow.modelData.id,
                                   !adapterRow.modelData.discoveryLeaseOwned)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: adapterRepeater.count === 0
        text: qsTr("No Bluetooth adapters are currently available.")
        muted: true
        Accessible.name: text
    }
}
