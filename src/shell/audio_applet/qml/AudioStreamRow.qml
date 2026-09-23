// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// One projected application-stream row, compact: name over direction, fader,
// monospace readout, mute. Stream moves are outside this slice, so the row
// offers only volume and mute requests.
RowLayout {
    id: root

    property var row: null
    property var controller: null

    readonly property string streamName: row?.label ?? ""
    readonly property bool pending: row?.pending ?? false

    spacing: Tokens.space["2"]

    Accessible.ignored: true

    ColumnLayout {
        Layout.preferredWidth: 112
        spacing: 0

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
            text: (root.row?.isPlayback ?? true) ? qsTr("Playing audio")
                                                 : qsTr("Recording audio")
            font: Qt.font({ family: Tokens.type.fontFamily,
                            pointSize: Tokens.type.caption })
            elide: Text.ElideRight
            muted: true
        }
    }

    C.Label {
        objectName: "audioStreamVolumeUnknown"
        visible: !(root.row?.volumeKnown ?? false)
        text: qsTr("Volume unknown")
        muted: true
    }

    C.Slider {
        id: volumeSlider
        objectName: "audioStreamVolume"

        readonly property double level: root.row?.volume ?? 0.0
        // AGENT-GUARD (ADR-0191): `pending` is not a gate here. See
        // AudioDeviceRow: a control that disabled itself while its own request
        // was in flight could not be dragged past one step.
        readonly property bool adjustable:
            (root.row?.canSetVolume ?? false) && (root.row?.volumeKnown ?? false)
                 && (root.controller?.controlGranted ?? false)

        visible: root.row?.volumeKnown ?? false
        Layout.fillWidth: true
        from: 0.0
        to: 1.0
        stepSize: 0.01
        wheelEnabled: false
        enabled: adjustable
        accessibleName: qsTr("Volume for %1").arg(root.streamName)
        accessibleDescription: root.pending
            ? qsTr("Volume change in progress")
            : !(root.controller?.controlGranted ?? false)
                  ? qsTr("Volume changes are not allowed for %1").arg(root.streamName)
                  : (root.row?.canSetVolume ?? false)
                        ? qsTr("Sets the volume from 0 to 100 percent")
                        : qsTr("This application stream does not allow volume changes")

        MouseArea {
            id: volumeWheel
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            enabled: volumeSlider.enabled && volumeSlider.visible
            property real remainder: 0
            property int targetSerial: Number(root.row?.serial ?? 0)
            onTargetSerialChanged: remainder = 0
            onEnabledChanged: if (!enabled) remainder = 0

            onWheel: wheel => {
                const angle = wheel.angleDelta
                const pixel = wheel.pixelDelta
                const hasAngle = angle.x !== 0 || angle.y !== 0
                const dx = hasAngle ? angle.x : pixel.x
                const dy = hasAngle ? angle.y : pixel.y
                const current = volumeSlider.value
                if (wheel.modifiers !== Qt.NoModifier || dy === 0
                        || Math.abs(dx) > Math.abs(dy)
                        || (dy > 0 && current >= volumeSlider.to - 1e-9)
                        || (dy < 0 && current <= volumeSlider.from + 1e-9)) {
                    remainder = 0
                    wheel.accepted = false
                    return
                }
                // AGENT-GUARD: retain sub-detent motion on this target only;
                // leave zero, horizontal, modified, and bound events to the
                // parent scroller. Pending does not disable adjustment.
                remainder += dy / (hasAngle ? 120 : 40)
                const steps = Math.trunc(remainder)
                if (steps === 0) {
                    wheel.accepted = true
                    return
                }
                remainder -= steps
                const next = Math.max(volumeSlider.from,
                    Math.min(volumeSlider.to,
                        current + steps * volumeSlider.stepSize))
                if (next === current) {
                    remainder = 0
                    wheel.accepted = false
                    return
                }
                if (next === volumeSlider.from || next === volumeSlider.to)
                    remainder = 0
                volumeSlider.value = next
                root.controller.requestVolume(root.row.serial, true, next)
                wheel.accepted = true
            }
        }

        // The only binding on `value`: a pressed control owns what it shows,
        // and `level` is the user's outstanding intent until the service
        // answers for it (ADR-0191).
        Binding {
            target: volumeSlider
            property: "value"
            value: volumeSlider.level
            when: !volumeSlider.pressed
            restoreMode: Binding.RestoreNone
        }

        // Every move dispatches; the controller coalesces latest-wins per
        // object. Qt reports pressed=true during keyboard steps, so `pressed`
        // cannot gate dispatch.
        onMoved: if (adjustable)
                     root.controller.requestVolume(root.row.serial, true, value)
    }

    C.Label {
        objectName: "audioStreamVolumePercent"
        visible: volumeSlider.visible
        // Follows the handle while it is held (ADR-0191); monospace so the
        // number never shuffles the fader as it changes width.
        text: Math.round(volumeSlider.value * 100) + "%"
        font: Qt.font({ family: Tokens.type.monoFontFamily,
                        pointSize: Tokens.type.caption })
        horizontalAlignment: Text.AlignRight
        Layout.preferredWidth: 40
        muted: true
    }

    C.Switch {
        id: muteSwitch
        objectName: "audioStreamMute"

        readonly property bool adjustable:
            (root.row?.canSetMute ?? false) && (root.row?.muteKnown ?? false)
                 && (root.controller?.controlGranted ?? false)

        visible: root.row?.muteKnown ?? false
        text: qsTr("Mute")
        checked: root.row?.muted ?? false
        enabled: adjustable
        accessibleDescription: root.pending
            ? qsTr("Mute change in progress")
            : qsTr("Mute state for %1").arg(root.streamName)
        onToggled: if (adjustable)
            root.controller.requestMute(root.row.serial, true, checked)
    }
}
