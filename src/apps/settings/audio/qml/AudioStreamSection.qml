// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk

// Per-application stream inventory. A device choice names one live output for
// playback or one live input for recording; only Audio1 readback changes the
// displayed selected target.
ColumnLayout {
    id: root

    required property var audioSettings
    required property var streamRows

    // Same focus registration contract as AudioDeviceSection: the target is
    // always an enabled, admitted control in traversal order (mute switch,
    // device picker, then volume row), never one the projection
    // disabled. See AudioDeviceSection for the host-entry AGENT-GUARD.
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
        title: qsTr("Application streams")
        description: qsTr("Levels and devices for open applications")
    }

    // AGENT-GUARD (mirrors ADR-0191, shell audio applet): index-stable rows,
    // same reasoning as AudioDeviceSection's Repeater above.
    Repeater {
        id: streamRepeater
        model: root.streamRows.length

        delegate: FormSurface {
            id: streamRow
            required property int index
            readonly property var modelData: root.streamRows[index] ?? null
            visible: modelData !== null
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1 stream %2, %3, %4, %5")
                .arg(streamRow.modelData?.directionText ?? "")
                .arg(streamRow.modelData?.applicationName ?? "")
                .arg(streamRow.modelData?.mediaName ?? "")
                .arg(streamRow.modelData?.targetName ?? "")
                .arg((streamRow.modelData?.volumeKnown ?? false)
                     ? qsTr("%1 percent")
                       .arg(streamRow.modelData.volumePercent)
                     : qsTr("volume unknown"))

            // Traversal order follows the visible controls and never hands
            // the Settings host a disabled or hidden focus target.
            readonly property Item firstEnabledAction:
                muteSwitch.enabled ? muteSwitch
                : targetPicker.entryControl.enabled ? targetPicker.entryControl
                : levelRow.entryControl.enabled ? levelRow.entryControl
                : null
            readonly property Item lastEnabledAction:
                levelRow.entryControl.enabled ? levelRow.entryControl
                : targetPicker.entryControl.enabled ? targetPicker.entryControl
                : muteSwitch.enabled ? muteSwitch : null

            onFirstEnabledActionChanged:
                root.updateActionRegistration(streamRow.index, streamRow)
            onLastEnabledActionChanged:
                root.updateActionRegistration(streamRow.index, streamRow)

            Component.onCompleted:
                root.updateActionRegistration(streamRow.index, streamRow)
            Component.onDestruction:
                root.removeActionRegistration(streamRow.index)

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
                            text: (streamRow.modelData?.mediaName.length ?? 0) > 0
                                  ? qsTr("%1 — %2")
                                    .arg(streamRow.modelData?.applicationName ?? "")
                                    .arg(streamRow.modelData?.mediaName ?? "")
                                  : streamRow.modelData?.applicationName ?? ""
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("%1 · %2")
                                .arg(streamRow.modelData?.directionText ?? "")
                                .arg(streamRow.modelData?.targetName ?? "")
                            muted: true
                        }
                    }

                    Label {
                        visible: streamRow.modelData?.muted ?? false
                        text: qsTr("Muted")
                        muted: true
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }

                    Switch {
                        id: muteSwitch
                        objectName: "audioStreamMute_"
                                    + (streamRow.modelData?.serial ?? 0)
                        text: qsTr("Mute")
                        checked: streamRow.modelData?.muted ?? false
                        enabled: streamRow.modelData?.muteAvailable ?? false
                        accessibleDescription: qsTr("Mute %1")
                            .arg(streamRow.modelData?.applicationName ?? "")
                        onToggled: streamRow.modelData !== null
                            && root.audioSettings.setStreamMuted(
                                   streamRow.modelData.serial, checked)
                    }
                }

                AudioStreamTargetPicker {
                    id: targetPicker
                    Layout.fillWidth: true
                    targetRow: streamRow.modelData
                    audioSettings: root.audioSettings
                }

                AudioLevelRow {
                    id: levelRow
                    Layout.fillWidth: true
                    targetRow: streamRow.modelData
                    kindPrefix: "audioStream"
                    targetName: streamRow.modelData?.applicationName ?? ""
                    commit: level => streamRow.modelData !== null
                        && root.audioSettings.setStreamVolume(
                               streamRow.modelData.serial, level)
                }
            }
        }
    }

    // AGENT-NOTE: themed through AudioPage's QindaQtTheme bridge.
    Tk.EmptyState {
        objectName: "audioStreamsEmpty"
        Layout.fillWidth: true
        visible: streamRepeater.count === 0
        iconName: "audio-lines"
        text: qsTr("No application streams are currently reported.")
    }
}
