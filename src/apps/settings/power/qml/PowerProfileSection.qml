// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var powerSettings
    // Checkpoint L row 5: injected AutomaticPowerProfilePolicyPort surface.
    // Options are built from the same live profileRows PPD reports, so the
    // AC/Battery/Low battery pickers never drift from the manual list above.
    required property var profilePolicy
    property Item firstActionTarget: null
    property var actionRegistrations: ({})
    // buildAutoOptions() stays pure like PowerLidPowerButtonSection's
    // buildOptions(): repeated binding evaluations never accumulate
    // duplicate out-of-set entries.
    function buildAutoOptions(currentId) {
        const options = [{ "value": "", "label": qsTr("Don't switch automatically") }]
        for (const row of root.powerSettings.profileRows)
            options.push({ "value": row.id, "label": row.label })
        if (currentId.length > 0
                && options.findIndex(function(option) {
                    return option.value === currentId }) < 0)
            options.push({ "value": currentId, "label": currentId })
        return options
    }
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
        title: qsTr("Automatic switching")
        description: qsTr("Let PowerDevil switch the power profile when the power source changes.")
    }

    FormSurface {
        Layout.fillWidth: true
        padding: Tokens.space["3"]
        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Automatic power profile switching")

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            component AutoProfileRow: RowLayout {
                id: autoRow
                required property string label
                required property string currentId
                required property string toolTipText
                required property string objectNameSuffix
                signal applied(string profileId)
                Layout.fillWidth: true
                enabled: root.profilePolicy.available && !root.profilePolicy.busy
                spacing: Tokens.space["2"]

                Label {
                    text: autoRow.label
                    Accessible.name: text
                    muted: !autoRow.enabled
                }
                ComboBox {
                    id: autoSelector
                    objectName: "powerAutoProfileSelector_" + autoRow.objectNameSuffix
                    Layout.fillWidth: true
                    enabled: autoRow.enabled
                    readonly property var options: root.buildAutoOptions(autoRow.currentId)
                    model: options
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: options.findIndex(function(option) {
                        return option.value === autoRow.currentId
                    })
                    T.ToolTip.visible: autoSelectorHover.hovered
                    T.ToolTip.delay: 600
                    T.ToolTip.text: autoRow.toolTipText
                    accessibleDescription: autoRow.toolTipText
                    onActivated: index => {
                        if (index >= 0 && index < options.length)
                            autoRow.applied(options[index].value)
                    }
                    HoverHandler { id: autoSelectorHover }
                }
            }

            AutoProfileRow {
                id: acAutoRow
                objectNameSuffix: "ac"
                label: qsTr("On AC power")
                currentId: root.profilePolicy.acProfileId
                toolTipText: qsTr("PowerDevil switches to this profile when AC power is connected")
                onApplied: profileId => root.profilePolicy.applyProfiles(
                    profileId, root.profilePolicy.batteryProfileId,
                    root.profilePolicy.lowBatteryProfileId)
            }
            AutoProfileRow {
                id: batteryAutoRow
                objectNameSuffix: "battery"
                label: qsTr("On battery")
                currentId: root.profilePolicy.batteryProfileId
                toolTipText: qsTr("PowerDevil switches to this profile when running on battery")
                onApplied: profileId => root.profilePolicy.applyProfiles(
                    root.profilePolicy.acProfileId, profileId,
                    root.profilePolicy.lowBatteryProfileId)
            }
            AutoProfileRow {
                id: lowBatteryAutoRow
                objectNameSuffix: "lowBattery"
                label: qsTr("On low battery")
                currentId: root.profilePolicy.lowBatteryProfileId
                toolTipText: qsTr("PowerDevil switches to this profile once the battery is low")
                onApplied: profileId => root.profilePolicy.applyProfiles(
                    root.profilePolicy.acProfileId, root.profilePolicy.batteryProfileId,
                    profileId)
            }

            Label {
                objectName: "powerAutoProfilePolicyError"
                Layout.fillWidth: true
                visible: root.profilePolicy.errorText.length > 0
                text: root.profilePolicy.errorText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }
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
