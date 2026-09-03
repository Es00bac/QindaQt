// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One device-kind inventory list with default-device selection, volume, and
// mute intents.
ColumnLayout {
    id: root

    required property var audioSettings
    required property string sectionTitle
    required property string sectionDescription
    required property var deviceRows
    required property string emptyText
    required property string kindPrefix

    // Projected rows register their first/last enabled, admitted action
    // control for the page's focus entry and Tab cycle. Registration is
    // keyed by row index so the section target is always the first enabled,
    // admitted control in traversal order — never a control the projection
    // disabled (AGENT-GUARD: the Settings host forceActiveFocus()es
    // firstActionTarget directly; handing it a disabled control strands host
    // entry focus). The Repeater recreates every delegate on each projection
    // change, so registration refreshes with each model reset; the
    // destroying delegate withdraws its row so a stale pointer is never
    // handed to forceActiveFocus.
    property Item firstActionTarget: null
    property Item lastActionTarget: null
    property var actionRegistrations: ({})

    function updateActionRegistration(index, delegate) {
        root.actionRegistrations[index] = delegate
        root.refreshActionTargets()
    }

    function removeActionRegistration(index) {
        delete root.actionRegistrations[index]
        root.refreshActionTargets()
    }

    function refreshActionTargets() {
        const indices = Object.keys(root.actionRegistrations).map(Number)
        indices.sort((a, b) => a - b)
        let first = null
        let last = null
        for (const index of indices) {
            const delegate = root.actionRegistrations[index]
            if (first === null && delegate.firstEnabledAction !== null) {
                first = delegate.firstEnabledAction
            }
            if (delegate.lastEnabledAction !== null) {
                last = delegate.lastEnabledAction
            }
        }
        root.firstActionTarget = first
        root.lastActionTarget = last
    }

    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: root.sectionTitle
        description: root.sectionDescription
    }

    Repeater {
        id: deviceRepeater
        model: root.deviceRows

        delegate: FormSurface {
            id: deviceRow
            required property var modelData
            required property int index
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1 %2, %3")
                .arg(deviceRow.modelData.kindText)
                .arg(deviceRow.modelData.displayName)
                .arg(deviceRow.modelData.stateText)

            // Traversal order inside a row is set-default, volume, mute; the
            // enabled flag already folds in admission (Button derives it from
            // available && !busy) and the projection's can-set fences.
            readonly property Item firstEnabledAction:
                setDefaultButton.visible && setDefaultButton.enabled
                    ? setDefaultButton
                    : levelRow.entryControl.enabled ? levelRow.entryControl
                    : muteSwitch.enabled ? muteSwitch : null
            readonly property Item lastEnabledAction:
                muteSwitch.enabled ? muteSwitch
                : levelRow.entryControl.enabled ? levelRow.entryControl
                : setDefaultButton.visible && setDefaultButton.enabled
                  ? setDefaultButton : null

            onFirstEnabledActionChanged:
                root.updateActionRegistration(deviceRow.index, deviceRow)
            onLastEnabledActionChanged:
                root.updateActionRegistration(deviceRow.index, deviceRow)

            Component.onCompleted:
                root.updateActionRegistration(deviceRow.index, deviceRow)
            Component.onDestruction:
                root.removeActionRegistration(deviceRow.index)

            contentItem: ColumnLayout {
                spacing: Tokens.space["2"]

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        Label {
                            Layout.fillWidth: true
                            text: deviceRow.modelData.displayName
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: deviceRow.modelData.stateText
                            muted: true
                        }
                    }

                    Label {
                        visible: deviceRow.modelData.isDefault
                        text: qsTr("Default")
                        muted: true
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }

                    Button {
                        id: setDefaultButton
                        objectName: root.kindPrefix + "Default_"
                                    + deviceRow.modelData.serial
                        visible: !deviceRow.modelData.isDefault
                        available: deviceRow.modelData.setDefaultAvailable
                        busy: root.audioSettings.busy
                        emphasized: false
                        text: qsTr("Set default")
                        accessibleDescription: qsTr("Make %1 the default %2")
                            .arg(deviceRow.modelData.displayName)
                            .arg(deviceRow.modelData.kindText)
                        onClicked: root.audioSettings.setDefaultDevice(
                                       deviceRow.modelData.serial)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["3"]

                    AudioLevelRow {
                        id: levelRow
                        Layout.fillWidth: true
                        targetRow: deviceRow.modelData
                        kindPrefix: root.kindPrefix
                        targetName: deviceRow.modelData.displayName
                        commit: level => root.audioSettings.setDeviceVolume(
                                     deviceRow.modelData.serial, level)
                    }

                    Switch {
                        id: muteSwitch
                        objectName: root.kindPrefix + "Mute_"
                                    + deviceRow.modelData.serial
                        text: qsTr("Mute")
                        checked: deviceRow.modelData.muted
                        enabled: deviceRow.modelData.muteAvailable
                        accessibleDescription: qsTr("Mute %1")
                            .arg(deviceRow.modelData.displayName)
                        onToggled: root.audioSettings.setDeviceMuted(
                                       deviceRow.modelData.serial, checked)
                    }
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: deviceRepeater.count === 0
        text: root.emptyText
        muted: true
    }
}
