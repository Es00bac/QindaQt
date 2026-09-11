// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Per-channel volume strip for one device: a fader and position label per
// projected channel. Availability comes from the route model's shared
// admission predicate and is never widened here; each fader commits exactly
// one channel of the retained layout.
ColumnLayout {
    id: root

    required property string targetName
    // Projected channel rows: {index, position, volumePercent, level01}.
    required property var channelRows
    // Commits one channel: commit(index, level).
    required property var commit
    required property bool available
    // 64-bit PipeWire serials do not fit QML number types; keep them var.
    required property var serial

    // The last enabled fader, or null; the owning device row nominates it as
    // its reverse-traversal action target while the strip is expanded.
    property Item lastEnabledControl: null

    function refreshLastEnabled() {
        let last = null
        for (let index = 0; index < channelFaders.count; ++index) {
            const row = channelFaders.itemAt(index)
            if (row !== null && row.slider !== null && row.slider.enabled) {
                last = row.slider
            }
        }
        root.lastEnabledControl = last
    }

    spacing: Tokens.space["2"]

    Repeater {
        id: channelFaders

        model: root.channelRows
        onCountChanged: root.refreshLastEnabled()
        onModelChanged: root.refreshLastEnabled()

        delegate: RowLayout {
            id: channelRow

            required property var modelData
            required property int index

            readonly property alias slider: channelSlider

            Layout.fillWidth: true
            spacing: Tokens.space["3"]

            Component.onCompleted: root.refreshLastEnabled()

            Label {
                Layout.preferredWidth: Tokens.space["6"] + Tokens.space["3"]
                text: channelRow.modelData.position
                muted: true
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            Slider {
                id: channelSlider
                objectName: "audioChannelVolume_" + root.serial + "_"
                             + channelRow.modelData.index
                Layout.fillWidth: true
                from: 0.0
                to: 1.0
                stepSize: 0.01
                value: channelRow.modelData.level01
                enabled: root.available
                accessibleName: qsTr("%1 %2 channel volume")
                    .arg(root.targetName)
                    .arg(channelRow.modelData.position)
                accessibleDescription: qsTr("Set %1 channel volume")
                    .arg(channelRow.modelData.position)
                // Same release-or-keyboard-step contract as the aggregate
                // volume row: a pointer drag stays quiet until it ends.
                onMoved: if (!pressed && enabled) {
                    root.commit(channelRow.modelData.index, value)
                }
                onEnabledChanged: root.refreshLastEnabled()
            }

            Label {
                objectName: "audioChannelVolumeText_" + root.serial + "_"
                            + channelRow.modelData.index
                Layout.preferredWidth: Tokens.space["6"]
                text: qsTr("%1%").arg(channelRow.modelData.volumePercent)
                muted: true
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }
    }
}
