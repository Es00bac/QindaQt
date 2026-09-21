// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// A vertical console fader with a printed dB scale and a readout, the way the
// reference console draws one: slot, fill from the floor up to the handle,
// numbered scale beside the slot, value under it.
//
// AGENT-NOTE: belongs in QindaTK as a vertical Fader with a scale; the
// toolkit's Slider is horizontal-only and carries no scale markings. Kept
// local because the QindaTK repository must not be changed from this lane.
//
// AGENT-CONTRACT: the fader is driven by POSITION and converted through the
// model's gain law, never by mapping dB in QML (ADR-0171). The scale labels
// are placed by the same `faderPositionForGain` and the readout reads the
// same `gainForFaderPosition`, so the number, the scale and the slot cannot
// disagree. qindaqt.settings-audio-page asserts both.
Item {
    id: fader

    required property var model
    required property real faderPosition
    property bool enabledControl: true
    property string accessibleName: ""

    signal moved(real position)

    implicitWidth: 72
    implicitHeight: 150
    activeFocusOnTab: enabledControl

    // Geometry the page test reads to check the scale against the gain law.
    readonly property real topY: 2
    readonly property real readoutHeight: 16
    readonly property real travel: Math.max(1, height - topY - readoutHeight - 2)
    readonly property real scaleWidth: 22
    readonly property real slotX: (width - scaleWidth) / 2

    // The drag edits this, not `faderPosition`: the projected binding must
    // stay intact so a model republish mid-drag cannot fight the pointer.
    property real livePosition: 0.0
    property bool commitPending: false
    Binding {
        target: fader
        property: "livePosition"
        value: fader.faderPosition
        when: !faderMouse.pressed && !fader.commitPending && !(fader.model.busy ?? false)
        restoreMode: Binding.RestoreNone
    }
    readonly property real fraction: Math.max(0.0, Math.min(1.0, livePosition))
    readonly property real gainDb: fader.model.gainForFaderPosition(fraction)
    readonly property string readout: (gainDb > 0 ? "+" : "")
        + (Math.round(gainDb * 10) / 10).toFixed(1) + qsTr(" dB")

    opacity: fader.enabledControl ? 1.0 : Tk.Theme.opacity.disabled
    Accessible.role: Accessible.Slider
    Accessible.name: fader.accessibleName.length > 0
        ? fader.accessibleName : fader.readout
    Accessible.description: fader.readout
    Accessible.onIncreaseAction: if (fader.enabledControl) fader.nudge(1)
    Accessible.onDecreaseAction: if (fader.enabledControl) fader.nudge(-1)

    // Keep only the latest point from this gesture while AudioClient has a
    // request in flight. Nothing is replayed after service loss or disable.
    function requestPosition(position) {
        if (!fader.enabledControl)
            return
        fader.commitPending = true
        fader.livePosition = clamp01(position)
        if (!commitTimer.running)
            commitTimer.start()
    }
    onEnabledControlChanged: {
        if (!enabledControl) {
            commitTimer.stop()
            commitPending = false
        }
    }
    Timer {
        id: commitTimer
        interval: 40
        onTriggered: {
            if (!fader.commitPending || !fader.enabledControl)
                return
            if (fader.model.busy ?? false) {
                restart()
                return
            }
            const position = fader.livePosition
            fader.moved(position)
            fader.commitPending = false
        }
    }

    function clamp01(v) { return Math.max(0.0, Math.min(1.0, v)) }
    function positionFromY(y, fine, fineStart) {
        if (fine) {
            return clamp01(fineStart + (fader._startY - y) / fader.travel * 0.1)
        }
        return clamp01(1.0 - (y - fader.topY) / fader.travel)
    }
    function nudge(steps) {
        requestPosition(fader.livePosition + steps * 0.02)
    }

    property real _startY: 0
    property real _startPosition: 0

    // Scale ticks at the standard gain steps; the unity tick (0 dB) is the
    // long one, as that is the position an operator finds by eye. Positions
    // come from the model's gain law — never a local dB mapping.
    Repeater {
        model: [12, 6, 0, -6, -12, -24, -36, -48, -60]
        delegate: Rectangle {
            required property real modelData
            readonly property real tickPosition: fader.model.faderPositionForGain(modelData)
            y: fader.topY + (1.0 - tickPosition) * fader.travel
            width: modelData === 0 ? 10 : 6
            height: 1
            x: fader.slotX - 12
            color: modelData === 0 ? Tk.Theme.color.text : Tk.Theme.color.divider
        }
    }

    // Numbered scale, printed beside the slot like the reference console.
    // Labels are placed by the same law as the ticks, so what the operator
    // reads is where the fader actually puts that gain.
    Repeater {
        model: [12, 0, -12, -24, -36, -48, -60]
        delegate: Text {
            required property real modelData
            readonly property real labelPosition: fader.model.faderPositionForGain(modelData)
            objectName: "consoleFaderScaleLabel_"
                + (modelData > 0 ? "p" + modelData : modelData < 0 ? "m" + (-modelData) : "0")
            y: fader.topY + (1.0 - labelPosition) * fader.travel - height / 2
            x: fader.width - fader.scaleWidth
            width: fader.scaleWidth
            height: 10
            text: (modelData > 0 ? "+" : "") + modelData
            color: modelData === 0 ? Tk.Theme.color.text : Tk.Theme.color.textMuted
            font.pixelSize: Tk.Theme.font.micro
            font.family: Tk.Theme.font.monoFamily
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    Rectangle {
        x: fader.slotX - 3
        y: fader.topY
        width: 6
        height: fader.travel
        radius: 3
        color: Tk.Theme.color.canvas
        border.width: Tk.Theme.size.border
        border.color: Tk.Theme.color.border
    }
    Rectangle {
        x: fader.slotX - 2
        y: fader.topY + (1.0 - fader.fraction) * fader.travel
        width: 4
        height: fader.fraction * fader.travel
        radius: 2
        color: Tk.Theme.color.accent
        opacity: 0.85
    }
    Rectangle {
        x: fader.slotX - width / 2
        y: fader.topY + (1.0 - fader.fraction) * fader.travel - height / 2
        width: 28
        height: 12
        radius: Tk.Theme.radius.xs
        color: faderMouse.pressed ? Tk.Theme.color.pressed : Tk.Theme.color.panel
        border.width: Tk.Theme.size.border
        border.color: fader.activeFocus ? Tk.Theme.color.accent : Tk.Theme.color.borderStrong
    }

    // The readout under the travel, where the reference console prints it.
    Text {
        objectName: "consoleFaderReadout"
        y: fader.topY + fader.travel + 2
        width: fader.width - fader.scaleWidth
        height: fader.readoutHeight
        text: fader.readout
        color: fader.enabledControl ? Tk.Theme.color.text : Tk.Theme.color.textDisabled
        font.pixelSize: Tk.Theme.font.small
        font.family: Tk.Theme.font.monoFamily
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        id: faderMouse
        anchors.fill: parent
        enabled: fader.enabledControl
        hoverEnabled: true
        cursorShape: enabled ? Qt.SizeVerCursor : Qt.ArrowCursor

        onPressed: mouse => {
            fader.forceActiveFocus(Qt.MouseFocusReason)
            fader._startY = mouse.y
            fader._startPosition = fader.livePosition
            fader.requestPosition(fader.positionFromY(
                        mouse.y, mouse.modifiers & Qt.ShiftModifier,
                        fader._startPosition))
        }
        onPositionChanged: mouse => {
            if (!pressed) {
                return
            }
            fader.requestPosition(fader.positionFromY(
                        mouse.y, mouse.modifiers & Qt.ShiftModifier,
                        fader._startPosition))
        }
        onDoubleClicked: {
            fader.requestPosition(fader.model.unityFaderPosition())
        }
        onWheel: wheel => {
            wheel.accepted = true
            fader.nudge(wheel.angleDelta.y > 0 ? 1 : -1)
        }
    }

    Keys.onPressed: event => {
        if (!fader.enabledControl)
            return
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right) {
            fader.nudge(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            fader.nudge(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            fader.requestPosition(fader.model.unityFaderPosition())
            event.accepted = true
        } else if (event.key === Qt.Key_End) {
            fader.requestPosition(0.0)
            event.accepted = true
        }
    }
}
