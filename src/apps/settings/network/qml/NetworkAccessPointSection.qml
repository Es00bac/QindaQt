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
        title: qsTr("Visible Wi-Fi networks")
        description: qsTr("Observed access points are read-only; unsaved networks cannot be configured here")
    }

    Repeater {
        id: accessPointRepeater
        model: root.networkSettings.accessPoints

        delegate: FormSurface {
            id: accessPointRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1, %2, signal %3 percent on %4")
                .arg(accessPointRow.modelData.displayName)
                .arg(accessPointRow.modelData.securityText)
                .arg(accessPointRow.modelData.signalStrength)
                .arg(accessPointRow.modelData.deviceInterface)

            contentItem: RowLayout {
                spacing: Tokens.space["3"]

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Label {
                        Layout.fillWidth: true
                        text: accessPointRow.modelData.displayName
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 · %2 · %3 MHz")
                            .arg(accessPointRow.modelData.securityText)
                            .arg(accessPointRow.modelData.deviceInterface)
                            .arg(accessPointRow.modelData.frequencyMHz)
                        muted: true
                    }
                }

                Label {
                    text: qsTr("%1%").arg(accessPointRow.modelData.signalStrength)
                    Accessible.name: qsTr("Signal strength %1 percent")
                        .arg(accessPointRow.modelData.signalStrength)
                }

                Label {
                    visible: accessPointRow.modelData.saved
                    text: qsTr("Saved")
                    muted: true
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: accessPointRepeater.count === 0
        text: qsTr("No access points are currently reported.")
        muted: true
    }
}
