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
        // The grant, not the row state alone, decides adjustability: a
        // policy-denied applet renders truth but can never dispatch.
        //
        // AGENT-GUARD (ADR-0191): `pending` is deliberately NOT here. A
        // control that disables itself while its own request is in flight
        // cannot be dragged: the first move dispatched, the row went pending,
        // the slider went dead, and the drag ended one step from where it
        // started. Pending is a subtle presentation state, never a gate.
        readonly property bool adjustable:
            (row?.canSetVolume ?? false) && (row?.volumeKnown ?? false)
                 && (controller?.controlGranted ?? false)

        visible: row?.volumeKnown ?? false
        Layout.preferredWidth: 140
        from: 0.0
        to: 1.0
        stepSize: 0.01
        wheelEnabled: true
        enabled: adjustable
        accessibleName: qsTr("Volume for %1").arg(root.deviceName)
        accessibleDescription: root.pending
            ? qsTr("Volume change in progress")
            : !(controller?.controlGranted ?? false)
                  ? qsTr("Volume changes are not allowed for %1").arg(root.deviceName)
                  : (row?.canSetVolume ?? false)
                        ? qsTr("Sets the volume from 0 to 100 percent")
                        : qsTr("This device does not allow volume changes")

        // AGENT-CONTRACT (ADR-0191): a pressed control owns its value. The
        // authoritative level rebinds only when the user is not holding the
        // handle, so an in-flight snapshot cannot yank the knob back under the
        // finger. On release the binding resumes and the next snapshot is
        // authoritative again.
        Binding {
            target: volumeSlider
            property: "value"
            value: volumeSlider.level
            when: !volumeSlider.pressed
            restoreMode: Binding.RestoreNone
        }

        // Every move dispatches; the controller coalesces latest-wins per
        // object with one request in flight, so a drag sends the value the
        // finger is on when the previous one completes rather than a queue of
        // stale steps. Qt reports pressed=true during keyboard steps too, so
        // `pressed` must not gate dispatch.
        onMoved: if (adjustable)
                     controller.requestVolume(row.serial, false, value)
    }

    C.Label {
        objectName: "audioDeviceVolumePercent"
        visible: volumeSlider.visible
        // Follows the handle while it is held, the authoritative level
        // otherwise: a readout that lagged the finger read as a stuck slider.
        text: Math.round(volumeSlider.value * 100) + "%"
        muted: true
    }

    C.Switch {
        id: muteSwitch
        objectName: "audioDeviceMute"

        // Mute is never gated on pending either, and the controller dispatches
        // a queued mute ahead of a queued volume (ADR-0191).
        readonly property bool adjustable:
            (row?.canSetMute ?? false) && (row?.muteKnown ?? false)
                 && (controller?.controlGranted ?? false)

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
