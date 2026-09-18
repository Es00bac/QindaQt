// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// One default-device band: a picker naming which device this band rides, a
// full-width fader, a monospace readout and mute. The row owns no state of its
// own beyond the pick — everything else is a projection of the controller's
// bounded device list, and a reprojection replaces the whole row object.
//
// AGENT-CONTRACT: the picker chooses which device THIS BAND controls. It does
// not change the system default, and it must not appear to: the applet's
// intent surface is closed to requestVolume/requestMute/requestStripFader/
// requestStripMute/clearFeedback, and docs/wiki/shell/audio-applet.md states
// that "this applet never sets defaults or moves streams". Changing the
// default is Settings' job.
ColumnLayout {
    id: root

    required property var controller
    // true for the output band, false for the input band.
    required property bool outputs
    // 0 rides whatever the service calls the default; a non-zero serial pins
    // this band to one device for as long as that device exists.
    required property int selectedSerial
    signal devicePicked(int serial)

    readonly property var rows: {
        const all = root.controller?.deviceRows ?? []
        const kept = []
        for (let i = 0; i < all.length; ++i) {
            if (all[i].isOutput === root.outputs)
                kept.push(all[i])
        }
        return kept
    }

    readonly property var defaultRow: {
        for (let i = 0; i < root.rows.length; ++i) {
            if (root.rows[i].isDefault === true)
                return root.rows[i]
        }
        return root.rows.length > 0 ? root.rows[0] : null
    }

    // AGENT-GUARD: a pinned device that has gone away falls back to the
    // default rather than leaving the band controlling nothing. The pick is
    // never silently rewritten — it simply stops resolving while absent, and
    // resolves again when the device returns.
    readonly property var row: {
        if (root.selectedSerial !== 0) {
            for (let i = 0; i < root.rows.length; ++i) {
                if (Number(root.rows[i].serial) === root.selectedSerial)
                    return root.rows[i]
            }
        }
        return root.defaultRow
    }

    readonly property string deviceName: row?.label ?? ""
    readonly property bool pending: row?.pending ?? false

    spacing: Tokens.space["1"]

    Accessible.role: Accessible.Grouping
    Accessible.name: root.outputs ? qsTr("Output device") : qsTr("Input device")

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        C.ComboBox {
            id: picker
            objectName: root.outputs ? "audioOutputPicker" : "audioInputPicker"

            readonly property var entries: {
                const list = []
                for (let i = 0; i < root.rows.length; ++i) {
                    const candidate = root.rows[i]
                    list.push({
                        serial: Number(candidate.serial),
                        label: candidate.isDefault === true
                            ? qsTr("%1 (default)").arg(candidate.label)
                            : candidate.label
                    })
                }
                return list
            }

            Layout.fillWidth: true
            model: entries
            textRole: "label"
            // A band with exactly one device still shows its name; greying
            // the picker there read as "this device is unavailable", which is
            // the opposite of true.
            enabled: entries.length > 0
            // AGENT-GUARD: the index is derived from the resolved row, never
            // stored, so a device appearing or disappearing cannot leave the
            // picker pointing at a row that is no longer there.
            currentIndex: {
                const serial = Number(root.row?.serial ?? 0)
                for (let i = 0; i < entries.length; ++i) {
                    if (entries[i].serial === serial)
                        return i
                }
                return -1
            }
            // AGENT-NOTE: QindaQt.Controls' ComboBox binds Accessible.name to
            // its display text and offers no accessibleName property, so the
            // identity is set directly here rather than through one.
            Accessible.name: root.outputs
                ? qsTr("Output device this panel controls")
                : qsTr("Input device this panel controls")
            accessibleDescription: qsTr(
                "Chooses which device these controls adjust; the system default is set in Settings")
            onActivated: index => root.devicePicked(entries[index].serial)

            // AGENT-GUARD: same defect as the console picker — the shared
            // ComboBox draws its closed face with a read-only TextField, which
            // scrolls a too-long string so the FIRST characters disappear. An
            // eliding Text drops characters from the end instead. This picker
            // is never editable.
            contentItem: Text {
                text: picker.displayText
                color: picker.enabled ? Tokens.fg.default : Tokens.fg.disabled
                font: picker.font
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        C.Switch {
            id: muteSwitch
            objectName: "audioDeviceMute"

            // Mute is never gated on pending, and the controller dispatches a
            // queued mute ahead of a queued volume (ADR-0191).
            readonly property bool adjustable:
                (root.row?.canSetMute ?? false) && (root.row?.muteKnown ?? false)
                     && (root.controller?.controlGranted ?? false)

            visible: root.row?.muteKnown ?? false
            text: qsTr("Mute")
            checked: root.row?.muted ?? false
            enabled: adjustable
            accessibleDescription: root.pending
                ? qsTr("Mute change in progress")
                : qsTr("Mute state for %1").arg(root.deviceName)
            onToggled: if (adjustable)
                root.controller.requestMute(root.row.serial, false, checked)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        C.Slider {
            id: volumeSlider
            objectName: "audioDeviceVolume"

            readonly property double level: root.row?.volume ?? 0.0
            // The grant, not the row state alone, decides adjustability: a
            // policy-denied applet renders truth but can never dispatch.
            //
            // AGENT-GUARD (ADR-0191): `pending` is deliberately NOT here. A
            // control that disables itself while its own request is in flight
            // cannot be dragged: the first move dispatched, the row went
            // pending, the slider went dead, and the drag ended one step from
            // where it started. Pending is a subtle presentation state, never
            // a gate.
            readonly property bool adjustable:
                (root.row?.canSetVolume ?? false) && (root.row?.volumeKnown ?? false)
                     && (root.controller?.controlGranted ?? false)

            visible: root.row?.volumeKnown ?? false
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            stepSize: 0.01
            wheelEnabled: true
            enabled: adjustable
            accessibleName: qsTr("Volume for %1").arg(root.deviceName)
            accessibleDescription: root.pending
                ? qsTr("Volume change in progress")
                : !(root.controller?.controlGranted ?? false)
                      ? qsTr("Volume changes are not allowed for %1").arg(root.deviceName)
                      : (root.row?.canSetVolume ?? false)
                            ? qsTr("Sets the volume from 0 to 100 percent")
                            : qsTr("This device does not allow volume changes")

            // AGENT-CONTRACT (ADR-0191): a pressed control owns its value. The
            // authoritative level rebinds only when the user is not holding
            // the handle, so an in-flight snapshot cannot yank the knob back
            // under the finger. On release the binding resumes and the next
            // snapshot is authoritative again.
            Binding {
                target: volumeSlider
                property: "value"
                value: volumeSlider.level
                when: !volumeSlider.pressed
                restoreMode: Binding.RestoreNone
            }

            // Every move dispatches; the controller coalesces latest-wins per
            // object with one request in flight, so a drag sends the value the
            // finger is on when the previous one completes rather than a queue
            // of stale steps. Qt reports pressed=true during keyboard steps
            // too, so `pressed` must not gate dispatch.
            onMoved: if (adjustable)
                         root.controller.requestVolume(root.row.serial, false, value)
        }

        C.Label {
            objectName: "audioDeviceVolumePercent"
            visible: volumeSlider.visible
            // A monospace readout so the number does not shuffle the fader as
            // it changes width. Follows the handle while it is held: a readout
            // that lagged the finger read as a stuck slider.
            text: Math.round(volumeSlider.value * 100) + "%"
            font: Qt.font({ family: Tokens.type.monoFontFamily,
                            pointSize: Tokens.type.caption })
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 40
            muted: true
        }

        C.Label {
            objectName: "audioDeviceVolumeUnknown"
            visible: !(root.row?.volumeKnown ?? false)
            text: qsTr("Volume unknown")
            muted: true
        }
    }
}
