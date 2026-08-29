// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
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
        title: qsTr("Arrangement & Primary Role")
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
        description: qsTr("Logical desktop offset coordinates in pixels")
        editor: positionRow

        RowLayout {
            id: positionRow
            spacing: Tokens.space["2"]

            TextField {
                id: posXField
                objectName: "displayPosXField"
                implicitWidth: 100
                text: root.posX.toString()
                enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                Accessible.name: qsTr("Position X coordinate")
                onEditingFinished: {
                    const parsedX = parseInt(text, 10);
                    if (!isNaN(parsedX) && root.displaySettings.selectedOutputId) {
                        root.displaySettings.setOutputPosition(
                            root.displaySettings.selectedOutputId, parsedX, root.posY);
                    }
                }
            }

            Label {
                text: "×"
                color: Tokens.fg.muted
            }

            TextField {
                id: posYField
                objectName: "displayPosYField"
                implicitWidth: 100
                text: root.posY.toString()
                enabled: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                Accessible.name: qsTr("Position Y coordinate")
                onEditingFinished: {
                    const parsedY = parseInt(text, 10);
                    if (!isNaN(parsedY) && root.displaySettings.selectedOutputId) {
                        root.displaySettings.setOutputPosition(
                            root.displaySettings.selectedOutputId, root.posX, parsedY);
                    }
                }
            }
        }
    }
}
