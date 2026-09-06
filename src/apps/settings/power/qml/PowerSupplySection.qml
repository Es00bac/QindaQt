// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var powerSettings
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Power supplies")
        description: qsTr("Batteries, UPS devices, adapters, estimates, and warning severity")
    }

    Repeater {
        id: supplyRepeater
        model: root.powerSettings.supplyRows

        delegate: FormSurface {
            id: supplyRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: supplyRow.modelData.name
            Accessible.description: supplyRow.modelData.accessibleDescription

            contentItem: GridLayout {
                objectName: "powerSupplyLayout_" + supplyRow.modelData.id
                columns: supplyRow.width < 500 ? 1 : 2
                columnSpacing: Tokens.space["4"]
                rowSpacing: Tokens.space["1"]

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]
                    Label {
                        Layout.fillWidth: true
                        text: supplyRow.modelData.name
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: supplyRow.modelData.kindText
                        muted: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]
                    Label {
                        objectName: "powerSupplyState_" + supplyRow.modelData.id
                        Layout.fillWidth: true
                        text: [supplyRow.modelData.stateText,
                               supplyRow.modelData.percentageText,
                               supplyRow.modelData.timeText]
                              .filter(value => value.length > 0).join(" · ")
                    }
                    Label {
                        objectName: "powerSupplyWarning_" + supplyRow.modelData.id
                        Layout.fillWidth: true
                        text: supplyRow.modelData.warningText
                        color: supplyRow.modelData.warningSeverity >= 4
                               ? Tokens.fg.default
                               : supplyRow.modelData.warningSeverity >= 3
                                 ? Tokens.fg.default
                                 : Tokens.fg.muted
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: supplyRepeater.count === 0
        text: qsTr("No power supplies are currently reported.")
        muted: true
        Accessible.name: text
    }
}
