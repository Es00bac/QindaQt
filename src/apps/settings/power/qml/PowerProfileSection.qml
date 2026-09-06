// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var powerSettings
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
        title: qsTr("Power mode")
        description: qsTr("Choose between power saving and performance.")
    }

    Repeater {
        id: profileRepeater
        model: root.powerSettings.profileRows

        delegate: Button {
            id: profileButton
            required property var modelData
            required property int index
            objectName: "powerProfile_" + profileButton.modelData.id
            Layout.fillWidth: true
            available: profileButton.modelData.available
            busy: root.powerSettings.busy
            emphasized: profileButton.modelData.active
            text: profileButton.modelData.active
                  ? qsTr("%1 (active)").arg(profileButton.modelData.label)
                  : profileButton.modelData.label
            accessibleDescription: profileButton.modelData.accessibleDescription
            Accessible.role: Accessible.RadioButton
            Accessible.checked: profileButton.modelData.active
            onEnabledChanged: root.updateAction(profileButton.index, profileButton)
            Component.onCompleted: root.updateAction(profileButton.index, profileButton)
            Component.onDestruction: root.removeAction(profileButton.index)
            onClicked: root.powerSettings.requestProfile(profileButton.modelData.id)
        }
    }

    Label {
        Layout.fillWidth: true
        visible: profileRepeater.count === 0
        text: qsTr("No selectable power profiles are currently reported.")
        muted: true
        Accessible.name: text
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Profile holds")
        description: qsTr("Read-only holds and the reasons applications requested them")
    }

    Repeater {
        id: holdRepeater
        model: root.powerSettings.profileHoldRows

        delegate: FormSurface {
            id: holdRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: qsTr("%1 holds %2")
                .arg(holdRow.modelData.applicationName)
                .arg(holdRow.modelData.profileId)
            Accessible.description: holdRow.modelData.accessibleDescription
            contentItem: ColumnLayout {
                spacing: Tokens.space["1"]
                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 · %2")
                        .arg(holdRow.modelData.applicationName)
                        .arg(holdRow.modelData.profileId)
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: holdRow.modelData.reason
                    wrapMode: Text.Wrap
                    muted: true
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: holdRepeater.count === 0
        text: qsTr("No applications currently hold a power profile.")
        muted: true
        Accessible.name: text
    }
}
