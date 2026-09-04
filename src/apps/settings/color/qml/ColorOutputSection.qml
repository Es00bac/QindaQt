// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var colorSettings
    property Item firstActionTarget: null
    property var actionRegistrations: ({})
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    function updateAction(index, item) {
        root.actionRegistrations[index] = item
        root.refreshTarget()
    }
    function removeAction(index) {
        delete root.actionRegistrations[index]
        root.refreshTarget()
    }
    function refreshTarget() {
        const indices = Object.keys(root.actionRegistrations).map(Number)
        indices.sort((left, right) => left - right)
        root.firstActionTarget = null
        for (const index of indices) {
            const item = root.actionRegistrations[index]
            if (item !== null && item.enabled) {
                root.firstActionTarget = item
                break
            }
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Displays")
        description: qsTr("Connected displays and the ICC profile currently assigned to each")
    }

    Repeater {
        id: outputRepeater
        model: root.colorSettings.outputRows

        delegate: Button {
            id: outputButton
            required property var modelData
            required property int index
            objectName: "colorOutput_" + outputButton.modelData.id
            Layout.fillWidth: true
            available: true
            busy: root.colorSettings.busy
            emphasized: outputButton.modelData.selected
            text: qsTr("%1 — %2").arg(outputButton.modelData.name)
                      .arg(outputButton.modelData.assignmentText)
            accessibleDescription: outputButton.modelData.selected
                ? qsTr("Selected display. %1").arg(outputButton.modelData.accessibleDescription)
                : qsTr("Select display. %1").arg(outputButton.modelData.accessibleDescription)
            Accessible.role: Accessible.RadioButton
            Accessible.checked: outputButton.modelData.selected
            onEnabledChanged: root.updateAction(outputButton.index, outputButton)
            Component.onCompleted: root.updateAction(outputButton.index, outputButton)
            Component.onDestruction: root.removeAction(outputButton.index)
            onClicked: root.colorSettings.selectOutput(outputButton.modelData.id)
        }
    }

    Label {
        Layout.fillWidth: true
        visible: outputRepeater.count === 0
        text: qsTr("No displays are currently reported.")
        muted: true
        Accessible.name: text
    }

    SectionHeader {
        Layout.fillWidth: true
        visible: inactiveRepeater.count > 0
        title: qsTr("Disconnected displays")
        description: qsTr("Assignments kept for displays that are not currently connected")
    }

    Repeater {
        id: inactiveRepeater
        model: root.colorSettings.inactiveAssignmentRows

        delegate: FormSurface {
            id: inactiveRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: qsTr("Disconnected display %1").arg(inactiveRow.modelData.id)
            Accessible.description: inactiveRow.modelData.accessibleDescription
            contentItem: ColumnLayout {
                spacing: Tokens.space["1"]
                Label {
                    Layout.fillWidth: true
                    text: inactiveRow.modelData.id
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Keeps profile %1").arg(inactiveRow.modelData.profileText)
                    wrapMode: Text.Wrap
                    muted: true
                }
            }
        }
    }
}
