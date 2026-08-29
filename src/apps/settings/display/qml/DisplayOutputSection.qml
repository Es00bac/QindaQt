// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Section displaying the list of connected monitors and primary display selection.
ColumnLayout {
    id: root

    required property var displaySettings
    required property bool editorBusy
    readonly property Item firstFocusTarget: outputRepeater.count > 0
                                             ? outputRepeater.itemAt(0) : null

    spacing: Tokens.space["3"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Displays")
        description: qsTr("Select a display to configure its resolution, scaling, and orientation")
    }

    Flow {
        id: outputFlow
        Layout.fillWidth: true
        spacing: Tokens.space["3"]

        Repeater {
            id: outputRepeater
            model: root.displaySettings.outputs

            delegate: DisplayOutputCard {
                id: card
                required property var modelData
                outputData: card.modelData
                selected: root.displaySettings.selectedOutputId === card.modelData.stableId
                canEdit: root.displaySettings.canEdit && !root.editorBusy
                onSelectedRequested: root.displaySettings.setSelectedOutputId(card.modelData.stableId)
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Enable display")
        description: qsTr("Turn this display output on or off")
        editor: enableSwitch

        Switch {
            id: enableSwitch
            objectName: "displayEnableSwitch"
            text: checked ? qsTr("Enabled") : qsTr("Disabled")
            checked: root.displaySettings.selectedOutput.enabled ?? false
            enabled: root.displaySettings.canEdit && !root.editorBusy
            onToggled: {
                if (root.displaySettings.selectedOutputId) {
                    root.displaySettings.setOutputEnabled(
                        root.displaySettings.selectedOutputId, checked)
                }
            }
        }
    }
}
