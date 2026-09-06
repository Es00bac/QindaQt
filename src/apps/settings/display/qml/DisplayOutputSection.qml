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
        description: qsTr("Select a display to configure it. Its number marks it in the arrangement below.")
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
                required property int index
                outputData: card.modelData
                ordinal: card.index + 1
                selected: root.displaySettings.selectedOutputId === card.modelData.stableId
                canEdit: root.displaySettings.canEdit && !root.editorBusy
                onSelectedRequested: root.displaySettings.setSelectedOutputId(card.modelData.stableId)
            }
        }
    }

    FormRow {
        objectName: "displayEnableFormRow"
        Layout.fillWidth: true
        label: qsTr("Enable display")
        description: (root.displaySettings.selectedOutput.enabled ?? false)
                     ? qsTr("This display is ready to configure.")
                     : qsTr("Turn on this connected display before configuring it.")
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
