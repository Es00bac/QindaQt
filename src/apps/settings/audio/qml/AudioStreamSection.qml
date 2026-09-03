// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Per-application stream inventory with volume and mute intents. Moving a
// stream between devices is intentionally not part of this route slice even
// when the service advertises the capability.
ColumnLayout {
    id: root

    required property var audioSettings
    required property var streamRows

    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Application streams")
        description: qsTr("Per-application playback and recording levels from Audio1")
    }

    Repeater {
        id: streamRepeater
        model: root.streamRows

        delegate: FormSurface {
            id: streamRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1 stream %2, %3, %4, %5")
                .arg(streamRow.modelData.directionText)
                .arg(streamRow.modelData.applicationName)
                .arg(streamRow.modelData.mediaName)
                .arg(streamRow.modelData.targetName)
                .arg(streamRow.modelData.volumeKnown
                     ? qsTr("%1 percent")
                       .arg(streamRow.modelData.volumePercent)
                     : qsTr("volume unknown"))

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
                            text: streamRow.modelData.mediaName.length > 0
                                  ? qsTr("%1 — %2")
                                    .arg(streamRow.modelData.applicationName)
                                    .arg(streamRow.modelData.mediaName)
                                  : streamRow.modelData.applicationName
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("%1 · %2")
                                .arg(streamRow.modelData.directionText)
                                .arg(streamRow.modelData.targetName)
                            muted: true
                        }
                    }

                    Label {
                        visible: streamRow.modelData.muted
                        text: qsTr("Muted")
                        muted: true
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }

                    Switch {
                        objectName: "audioStreamMute_"
                                    + streamRow.modelData.serial
                        text: qsTr("Mute")
                        checked: streamRow.modelData.muted
                        enabled: streamRow.modelData.muteAvailable
                        accessibleDescription: qsTr("Mute %1")
                            .arg(streamRow.modelData.applicationName)
                        onToggled: root.audioSettings.setStreamMuted(
                                       streamRow.modelData.serial, checked)
                    }
                }

                AudioLevelRow {
                    Layout.fillWidth: true
                    targetRow: streamRow.modelData
                    kindPrefix: "audioStream"
                    targetName: streamRow.modelData.applicationName
                    commit: level => root.audioSettings.setStreamVolume(
                                 streamRow.modelData.serial, level)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: streamRepeater.count === 0
        text: qsTr("No application streams are currently reported.")
        muted: true
    }
}
