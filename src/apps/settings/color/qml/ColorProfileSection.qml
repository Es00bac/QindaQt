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
        title: root.colorSettings.selectedOutputName.length > 0
               ? qsTr("Profiles for %1").arg(root.colorSettings.selectedOutputName)
               : qsTr("Profiles")
        description: qsTr("Assign one discovered ICC profile to the selected display")
    }

    Button {
        id: unassignButton
        objectName: "colorUnassignButton"
        Layout.fillWidth: true
        visible: root.colorSettings.selectedOutputId.length > 0
        available: root.colorSettings.unassignAvailable
        busy: root.colorSettings.busy
        emphasized: false
        text: qsTr("Remove assignment")
        accessibleDescription: qsTr("Remove the profile assignment from the selected display")
        onEnabledChanged: root.updateAction(-1, unassignButton)
        Component.onCompleted: root.updateAction(-1, unassignButton)
        Component.onDestruction: root.removeAction(-1)
        onClicked: root.colorSettings.unassignSelected()
    }

    Repeater {
        id: profileRepeater
        model: root.colorSettings.profileRows

        delegate: Button {
            id: profileButton
            required property var modelData
            required property int index
            objectName: "colorProfile_" + profileButton.modelData.id
            Layout.fillWidth: true
            available: profileButton.modelData.available
            busy: root.colorSettings.busy
            emphasized: profileButton.modelData.assigned
            text: profileButton.modelData.assigned
                  ? qsTr("%1 (assigned)").arg(profileButton.modelData.name)
                  : profileButton.modelData.name
            accessibleDescription: profileButton.modelData.accessibleDescription
            Accessible.role: Accessible.RadioButton
            Accessible.checked: profileButton.modelData.assigned
            onEnabledChanged: root.updateAction(profileButton.index, profileButton)
            Component.onCompleted: root.updateAction(profileButton.index, profileButton)
            Component.onDestruction: root.removeAction(profileButton.index)
            onClicked: root.colorSettings.assignProfile(profileButton.modelData.id)
        }
    }

    Label {
        Layout.fillWidth: true
        visible: root.colorSettings.selectedOutputId.length === 0
        text: qsTr("Select a display to assign a profile.")
        muted: true
        Accessible.name: text
    }

    Label {
        Layout.fillWidth: true
        visible: root.colorSettings.selectedOutputId.length > 0
                 && profileRepeater.count === 0
        text: qsTr("No discovered profiles are available to assign.")
        muted: true
        Accessible.name: text
    }
}
