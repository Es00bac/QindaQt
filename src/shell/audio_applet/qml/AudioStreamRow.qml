// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

// One projected application-stream row. Stream moves are outside this slice,
// so the row offers only volume and mute requests.
RowLayout {
    id: root

    property var row: null
    property var controller: null

    readonly property string streamName: row?.label ?? ""
    readonly property bool pending: row?.pending ?? false

    spacing: 12

    Accessible.ignored: true

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        C.Label {
            objectName: "audioStreamName"
            Layout.fillWidth: true
            text: root.streamName
            elide: Text.ElideRight
            muted: root.pending
        }

        C.Label {
            objectName: "audioStreamDirection"
            Layout.fillWidth: true
            text: (row?.isPlayback ?? true) ? qsTr("Playing audio")
                                            : qsTr("Recording audio")
            muted: true
        }
    }

    C.Label {
        objectName: "audioStreamVolumeUnknown"
        visible: !(row?.volumeKnown ?? false)
        text: qsTr("Volume unknown")
        muted: true
    }

    C.Slider {
        id: volumeSlider
        objectName: "audioStreamVolume"

        readonly property bool adjustable:
            (row?.canSetVolume ?? false) && (row?.volumeKnown ?? false)
                 && (controller?.controlGranted ?? false) && !root.pending

        visible: row?.volumeKnown ?? false
        Layout.preferredWidth: 140
        from: 0.0
        to: 1.0
        stepSize: 0.05
        value: row?.volume ?? 0.0
        enabled: adjustable
        accessibleName: qsTr("Volume for %1").arg(root.streamName)
        accessibleDescription: root.pending
            ? qsTr("Volume change in progress")
            : !(controller?.controlGranted ?? false)
                  ? qsTr("Volume changes are not allowed for %1").arg(root.streamName)
                  : (row?.canSetVolume ?? false)
                        ? qsTr("Sets the volume from 0 to 100 percent")
                        : qsTr("This application stream does not allow volume changes")

        // See AudioDeviceRow: dispatch on every moved; the row's pending
        // state disables the slider, and Qt 6.11 pressed=true during keyboard
        // steps makes `pressed` unusable as a dispatch gate.
        onMoved: if (adjustable)
                     controller.requestVolume(row.serial, true, value)
    }

    C.Label {
        objectName: "audioStreamVolumePercent"
        visible: volumeSlider.visible
        text: Math.round((row?.volume ?? 0.0) * 100) + "%"
        muted: true
    }

    C.Switch {
        id: muteSwitch
        objectName: "audioStreamMute"

        readonly property bool adjustable:
            (row?.canSetMute ?? false) && (row?.muteKnown ?? false)
                 && (controller?.controlGranted ?? false) && !root.pending

        visible: row?.muteKnown ?? false
        text: qsTr("Mute")
        checked: row?.muted ?? false
        enabled: adjustable
        accessibleDescription: root.pending
            ? qsTr("Mute change in progress")
            : qsTr("Mute state for %1").arg(root.streamName)
        onToggled: if (adjustable)
            controller.requestMute(row.serial, true, checked)
    }
}
