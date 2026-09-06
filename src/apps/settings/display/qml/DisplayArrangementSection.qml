// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Section for primary display role and relative multi-monitor positioning.
ColumnLayout {
    id: root

    required property var displaySettings
    required property bool editorBusy

    readonly property bool isPrimary: root.displaySettings.selectedOutput.primary ?? false
    readonly property bool outputEnabled: root.displaySettings.selectedOutput.enabled ?? false
    readonly property int posX: root.displaySettings.selectedOutput.positionX ?? 0
    readonly property int posY: root.displaySettings.selectedOutput.positionY ?? 0

    spacing: Tokens.space["3"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Arrangement")
        description: qsTr("Set the main display for panels and dialogs, and arrange monitor positions")
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Primary display")
        description: qsTr("Designate this monitor as the primary workspace display")
        editor: primarySwitch

        Switch {
            id: primarySwitch
            objectName: "displayPrimarySwitch"
            text: checked ? qsTr("Primary Display") : qsTr("Secondary Display")
            checked: root.isPrimary
            enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled && !root.isPrimary
            onToggled: {
                if (checked && root.displaySettings.selectedOutputId) {
                    root.displaySettings.setOutputPrimary(root.displaySettings.selectedOutputId)
                }
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Position (X, Y)")
        description: qsTr("Set where this display sits relative to the others. Changes apply after you choose Apply.")
        editor: positionRow

        RowLayout {
            id: positionRow
            spacing: Tokens.space["2"]

            DisplayCoordinateField {
                id: posXField
                objectName: "displayPosXField"
                implicitWidth: 100
                outputId: root.displaySettings.selectedOutputId
                authoritativeValue: root.posX
                coordinateName: qsTr("Position X coordinate")
                enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                onValidCommitRequested: (originOutputId, value) => {
                    if (originOutputId === root.displaySettings.selectedOutputId) {
                        root.displaySettings.setOutputPosition(
                            originOutputId, value, root.posY)
                    }
                }
            }

            Label {
                text: "×"
                color: Tokens.fg.muted
            }

            DisplayCoordinateField {
                id: posYField
                objectName: "displayPosYField"
                implicitWidth: 100
                outputId: root.displaySettings.selectedOutputId
                authoritativeValue: root.posY
                coordinateName: qsTr("Position Y coordinate")
                enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                onValidCommitRequested: (originOutputId, value) => {
                    if (originOutputId === root.displaySettings.selectedOutputId) {
                        root.displaySettings.setOutputPosition(
                            originOutputId, root.posX, value)
                    }
                }
            }
        }
    }
}
