// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// A vertical console fader: slot, fill from the floor up to the handle, and a
// round handle that carries the dB readout so no separate legend row is
// needed. Tick marks at the standard gain steps stand in for a scale.
//
// AGENT-CONTRACT: the fader is driven by POSITION and converted through the
// model's gain law, never by mapping dB linearly onto the slot. The handle
// readout reads the same conversion, so the number and the slot cannot
// disagree.
Item {
    id: fader

    required property var model
    required property real faderPosition
    property bool enabledControl: true
    property string accessibleName: ""

    signal moved(real position)

    implicitWidth: 40
    implicitHeight: 150

    readonly property real topY: 2
    readonly property real travel: Math.max(1, height - topY * 2)
    readonly property real slotX: width / 2

    // The drag edits this, not `faderPosition`: the projected binding must
    // stay intact so a model republish mid-drag cannot fight the pointer.
    property real livePosition: fader.faderPosition
    readonly property real fraction: Math.max(0.0, Math.min(1.0, livePosition))
    readonly property real gainDb: fader.model.gainForFaderPosition(fraction)
    readonly property string readout: Math.round(gainDb * 10) / 10

    opacity: fader.enabledControl ? 1.0 : 0.45
    Accessible.role: Accessible.Slider
    Accessible.name: fader.accessibleName.length > 0
        ? fader.accessibleName : fader.readout + qsTr(" decibels")
    Accessible.description: fader.readout + qsTr(" decibels")

    function clamp01(v) { return Math.max(0.0, Math.min(1.0, v)) }
    function positionFromY(y, fine, fineStart) {
        if (fine) {
            return clamp01(fineStart + (fader._startY - y) / fader.travel * 0.1)
        }
        return clamp01(1.0 - (y - fader.topY) / fader.travel)
    }
    function nudge(steps) {
        fader.livePosition = clamp01(fader.livePosition + steps * 0.02)
        fader.moved(fader.livePosition)
    }

    property real _startY: 0
    property real _startPosition: 0

    // Scale ticks at the standard gain steps; the unity tick (0 dB) is the
    // long one, as that is the position an operator finds by eye.
    Repeater {
        model: [12, 0, -12, -24, -36, -48]
        delegate: Rectangle {
            required property real modelData
            readonly property real tickPosition: fader.model.faderPositionForGain(modelData)
            y: fader.topY + (1.0 - tickPosition) * fader.travel
            width: modelData === 0 ? 12 : 7
            height: 1
            x: fader.slotX - 14
            color: modelData === 0 ? Tokens.outline.strong : Tokens.outline.divider
        }
    }

    Rectangle {
        x: fader.slotX - 4
        y: fader.topY
        width: 8
        height: fader.travel
        radius: 4
        color: Tokens.bg.base
    }
    Rectangle {
        x: fader.slotX - 3
        y: fader.topY + (1.0 - fader.fraction) * fader.travel
        width: 6
        height: fader.fraction * fader.travel
        radius: 3
        color: Tokens.accent.default
        opacity: 0.85
    }
    Rectangle {
        x: fader.slotX - width / 2
        y: fader.topY + (1.0 - fader.fraction) * fader.travel - height / 2
        width: 34
        height: 18
        radius: Tokens.radius.s
        color: faderMouse.pressed ? Qt.darker(Tokens.bg.raised, 1.25) : Tokens.bg.raised
        border.width: 1
        border.color: fader.activeFocus ? Tokens.accent.default : Tokens.outline.strong

        Label {
            anchors.fill: parent
            text: fader.readout
            font: Qt.font({ family: Tokens.type.monoFontFamily, pointSize: Tokens.type.caption })
            color: fader.enabledControl ? Tokens.fg.default : Tokens.fg.muted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
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
            fader.livePosition = fader.positionFromY(
                        mouse.y, mouse.modifiers & Qt.ShiftModifier,
                        fader._startPosition)
            fader.moved(fader.livePosition)
        }
        onPositionChanged: mouse => {
            if (!pressed) {
                return
            }
            fader.livePosition = fader.positionFromY(
                        mouse.y, mouse.modifiers & Qt.ShiftModifier,
                        fader._startPosition)
            fader.moved(fader.livePosition)
        }
        onDoubleClicked: {
            fader.livePosition = fader.model.unityFaderPosition()
            fader.moved(fader.livePosition)
        }
        onWheel: wheel => {
            wheel.accepted = true
            fader.nudge(wheel.angleDelta.y > 0 ? 1 : -1)
        }
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right) {
            fader.nudge(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            fader.nudge(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            fader.livePosition = fader.model.unityFaderPosition()
            fader.moved(fader.livePosition)
            event.accepted = true
        } else if (event.key === Qt.Key_End) {
            fader.livePosition = 0.0
            fader.moved(fader.livePosition)
            event.accepted = true
        }
    }
}
