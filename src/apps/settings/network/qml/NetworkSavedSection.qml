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
        title: qsTr("Saved networks")
        description: qsTr("Connect only to profiles already stored by NetworkManager")
    }

    Repeater {
        id: savedRepeater
        model: root.networkSettings.knownNetworks

        delegate: FormSurface {
            id: savedRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("Saved network %1, %2%3")
                .arg(savedRow.modelData.displayName)
                .arg(savedRow.modelData.securityText)
                .arg(savedRow.modelData.active ? qsTr(", connected") : "")

            contentItem: RowLayout {
                spacing: Tokens.space["3"]

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Label {
                        Layout.fillWidth: true
                        text: savedRow.modelData.displayName
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: savedRow.modelData.active
                              ? qsTr("Connected on %1 · %2")
                                    .arg(savedRow.modelData.activeDeviceInterface)
                                    .arg(savedRow.modelData.securityText)
                              : savedRow.modelData.securityText
                        muted: true
                    }
                }

                Label {
                    visible: savedRow.modelData.active
                    text: qsTr("Connected")
                    Accessible.name: text
                }

                Button {
                    objectName: "networkConnect_" + savedRow.modelData.id
                    visible: !savedRow.modelData.active
                    available: savedRow.modelData.connectAvailable
                    busy: root.networkSettings.busy
                    text: qsTr("Connect")
                    accessibleDescription: savedRow.modelData.mayRequireExternalCredentials
                        ? qsTr("Connect to this saved network. A password prompt appears if needed.")
                        : qsTr("Connect saved network")
                    onClicked: root.networkSettings.connectKnownNetwork(
                                   savedRow.modelData.id)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: savedRepeater.count === 0
        text: qsTr("No saved networks are available.")
        muted: true
    }
}
