// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C

// One projected output/input device row. The row owns no state of its own:
// everything is a projection of the controller's bounded device list, and a
// reprojection replaces the whole row object.
RowLayout {
    id: root

    property var row: null
    property var controller: null

    readonly property string deviceName: row?.label ?? ""
    readonly property bool pending: row?.pending ?? false

    spacing: 12

    Accessible.ignored: true

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        C.Label {
            objectName: "audioDeviceName"
            Layout.fillWidth: true
            text: root.deviceName
            elide: Text.ElideRight
            muted: root.pending
        }

        C.Label {
            objectName: "audioDeviceBadge"
            Layout.fillWidth: true
            visible: row?.isDefault ?? false
            text: qsTr("Default device")
            muted: true
        }
    }

    C.Label {
        objectName: "audioDeviceVolumeUnknown"
        visible: !(row?.volumeKnown ?? false)
        text: qsTr("Volume unknown")
        muted: true
    }

    C.Slider {
        id: volumeSlider
        objectName: "audioDeviceVolume"

        readonly property double level: row?.volume ?? 0.0
        readonly property bool adjustable:
            (row?.canSetVolume ?? false) && (row?.volumeKnown ?? false)
                 && !root.pending

        visible: row?.volumeKnown ?? false
        Layout.preferredWidth: 140
        from: 0.0
        to: 1.0
        stepSize: 0.05
        value: level
        enabled: adjustable
        accessibleName: qsTr("Volume for %1").arg(root.deviceName)
        accessibleDescription: adjustable
            ? qsTr("Sets the volume from 0 to 100 percent")
            : root.pending ? qsTr("Volume change in progress")
                           : qsTr("This device does not allow volume changes")
        // Dispatch on release or on each keyboard step. Keyboard steps arrive
        // with pressed already false, so the keyboard path requests
        // immediately while a pointer drag stays quiet until it ends.
        onMoved: if (!pressed && adjustable)
                     controller.requestVolume(row.serial, false, value)
    }

    C.Label {
        objectName: "audioDeviceVolumePercent"
        visible: volumeSlider.visible
        text: Math.round((row?.volume ?? 0.0) * 100) + "%"
        muted: true
    }

    C.Switch {
        id: muteSwitch
        objectName: "audioDeviceMute"

        readonly property bool adjustable:
            (row?.canSetMute ?? false) && (row?.muteKnown ?? false)
                 && !root.pending

        visible: row?.muteKnown ?? false
        text: qsTr("Mute")
        checked: row?.muted ?? false
        enabled: adjustable
        accessibleDescription: root.pending
            ? qsTr("Mute change in progress")
            : qsTr("Mute state for %1").arg(root.deviceName)
        onToggled: if (adjustable)
            controller.requestMute(row.serial, false, checked)
    }
}
