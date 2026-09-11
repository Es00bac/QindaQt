// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Managed virtual devices (Audio1 schema v2): create virtual output/input
// devices applications select like hardware, and remove only devices the
// service itself manages. Hardware inventory never appears here, so no
// hardware row can ever offer a remove action.
ColumnLayout {
    id: root

    required property var audioSettings
    required property var virtualDeviceRows

    // Same focus registration contract as AudioDeviceSection: the target is
    // always an enabled, admitted control in traversal order. See
    // AudioDeviceSection for the host-entry AGENT-GUARD.
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
            if (delegate === null || delegate === undefined) {
                continue
            }
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
        title: qsTr("Virtual devices")
        description: qsTr(
            "Extra software devices for streaming, recording, and app-to-app audio")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Label {
            Layout.fillWidth: true
            visible: virtualRepeater.count === 0
            text: qsTr("No virtual devices have been created.")
            muted: true
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        AudioIconButton {
            id: addOutputButton

            objectName: "audioVirtualAddOutput"
            iconName: "list-add"
            toolTip: qsTr("Add a virtual output device")
            available: root.audioSettings.canManageVirtualDevices
            onClicked: root.audioSettings.createVirtualDevice(
                           "output", qsTr("Virtual output"), 2)
        }

        AudioIconButton {
            id: addInputButton

            objectName: "audioVirtualAddInput"
            iconName: "list-add"
            toolTip: qsTr("Add a virtual input device")
            available: root.audioSettings.canManageVirtualDevices
            onClicked: root.audioSettings.createVirtualDevice(
                           "input", qsTr("Virtual input"), 2)
        }
    }

    // Traversal order for the section header row is the two create actions;
    // per-device rows then follow with their remove action.
    readonly property Item firstEnabledAction:
        addOutputButton.enabled ? addOutputButton
        : addInputButton.enabled ? addInputButton : null
    readonly property Item lastEnabledAction:
        virtualRepeater.count > 0 ? null
        : addInputButton.enabled ? addInputButton
        : addOutputButton.enabled ? addOutputButton : null

    onFirstEnabledActionChanged: root.updateActionRegistration(-1, root)
    onLastEnabledActionChanged: root.updateActionRegistration(-1, root)
    Component.onCompleted: root.updateActionRegistration(-1, root)
    Component.onDestruction: root.removeActionRegistration(-1)

    Repeater {
        id: virtualRepeater

        model: root.virtualDeviceRows

        delegate: FormSurface {
            id: virtualRow
            required property var modelData
            required property int index
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1 %2, %3")
                .arg(virtualRow.modelData.kindText)
                .arg(virtualRow.modelData.displayName)
                .arg(virtualRow.modelData.channelMap.length > 0
                     ? virtualRow.modelData.channelMap
                     : qsTr("%1 channels")
                       .arg(virtualRow.modelData.channelCount))

            readonly property Item firstEnabledAction:
                removeButton.enabled ? removeButton : null
            readonly property Item lastEnabledAction:
                removeButton.enabled ? removeButton : null

            onFirstEnabledActionChanged:
                root.updateActionRegistration(virtualRow.index, virtualRow)
            onLastEnabledActionChanged:
                root.updateActionRegistration(virtualRow.index, virtualRow)
            Component.onCompleted:
                root.updateActionRegistration(virtualRow.index, virtualRow)
            Component.onDestruction:
                root.removeActionRegistration(virtualRow.index)

            contentItem: RowLayout {
                spacing: Tokens.space["2"]

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Label {
                        Layout.fillWidth: true
                        text: virtualRow.modelData.displayName
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: virtualRow.modelData.channelMap.length > 0
                              ? qsTr("%1 · %2")
                                    .arg(virtualRow.modelData.kindText)
                                    .arg(virtualRow.modelData.channelMap)
                              : virtualRow.modelData.kindText
                        muted: true
                    }
                }

                AudioIconButton {
                    id: removeButton

                    objectName: "audioVirtualRemove_"
                                + virtualRow.modelData.serial
                    iconName: "edit-delete"
                    toolTip: qsTr("Remove %1")
                        .arg(virtualRow.modelData.displayName)
                    destructive: true
                    // Row truth AND snapshot capability: hardware rows never
                    // carry removeAvailable, and a section-level capability
                    // loss disables the action without hiding the inventory.
                    available: virtualRow.modelData.removeAvailable
                               && root.audioSettings.canManageVirtualDevices
                    onClicked: root.audioSettings.removeVirtualDevice(
                                   virtualRow.modelData.serial)
                }
            }
        }
    }
}
