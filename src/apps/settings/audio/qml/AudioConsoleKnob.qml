// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// A rotary rack knob: drag vertically to turn, double-click restores the
// block default, and the value commits ONCE on release so a drag is one
// operation rather than a stream of them (the rack is judged whole by the
// service; see ADR-0179).
//
// The dial is drawn from plain items — an LED tick ring lit up to the value
// plus a rotating needle — rather than Canvas: Canvas content does not paint
// under offscreen capture (the page-test grab path), and the tick ring reads
// better at 34px anyway.
//
// AGENT-CONTRACT: `value` is projected truth owned by the model. The knob
// only *displays* `liveValue` while a drag is in flight and emits
// `committed` when the gesture ends; it never writes `value` itself.
Item {
    id: knob

    required property string label
    required property real from
    required property real to
    required property real value
    property string unit: ""
    property int decimals: 0
    property bool enabledControl: true
    property real defaultValue: (knob.from + knob.to) / 2.0

    signal committed(real value)

    implicitWidth: 52
    implicitHeight: 62

    readonly property real range: knob.to - knob.from
    readonly property real fraction: knob.range > 0
        ? Math.max(0.0, Math.min(1.0, (knob.liveValue - knob.from) / knob.range))
        : 0.0

    // The drag edits this, not `value`: the projected binding must stay
    // intact so a model republish during a drag cannot fight the pointer.
    property real liveValue: knob.value
    property real _dragStartValue: 0.0
    property real _dragStartY: 0.0

    opacity: knob.enabledControl ? 1.0 : 0.45
    Accessible.role: Accessible.Slider
    Accessible.name: knob.label
    Accessible.description: qsTr("%1 %2")
        .arg(Number(knob.liveValue).toFixed(knob.decimals)).arg(knob.unit)

    function clamped(v) {
        return Math.max(knob.from, Math.min(knob.to, v))
    }
    function stepBy(deltaSteps) {
        const step = knob.range / 40.0
        knob.liveValue = clamped(knob.liveValue + deltaSteps * step)
        knob.committed(knob.liveValue)
    }

    Column {
        anchors.fill: parent
        spacing: 0

        T.Label {
            width: parent.width
            height: 12
            text: knob.label
            color: Tokens.fg.muted
            font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        // The dial. Tick ring first (lit ticks are the value readout), then
        // the well, then the needle on top.
        Item {
            id: dial

            readonly property real diameter: Math.min(parent.width, knob.implicitHeight - 24)

            width: parent.width
            height: knob.implicitHeight - 24

            Repeater {
                model: 21
                delegate: Rectangle {
                    required property int index
                    readonly property real tickAngle: -135 + index * 270 / 20
                    readonly property bool lit: index / 20 <= knob.fraction + 0.001
                    width: 2
                    height: tickAngle % 90 === 0 ? 5 : 3
                    radius: 1
                    x: dial.width / 2 - 1
                    y: dial.height / 2 - dial.diameter / 2 - 1
                    transform: Rotation {
                        origin.x: 1
                        origin.y: dial.diameter / 2 + 1
                        angle: tickAngle
                    }
                    color: lit ? Tokens.accent.default : Tokens.outline.divider
                    opacity: lit ? 1.0 : 0.6
                }
            }

            // The value halo: a ring that brightens as the dial moves away
            // from its block default, so a rack that has been touched reads as
            // touched at a glance without having to compare numbers. Drawn as
            // a plain bordered Rectangle, never a Canvas — Canvas content does
            // not paint under offscreen capture, which is how this surface is
            // reviewed.
            Rectangle {
                anchors.centerIn: parent
                width: dial.diameter - 4
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: Tokens.accent.default
                visible: knob.range > 0
                opacity: 0.55 * Math.min(1.0,
                    Math.abs(knob.liveValue - knob.defaultValue) / (knob.range / 2))
            }

            Rectangle {
                anchors.centerIn: parent
                width: dial.diameter - 10
                height: width
                radius: width / 2
                color: Tokens.bg.base
                border.width: Tokens.space["1"] / 2
                border.color: knob.activeFocus ? Tokens.accent.default : Tokens.outline.strong
            }

            // Needle: rotates about the dial centre; -135° points down-left,
            // +135° down-right, 0° straight up — matching the tick ring.
            Item {
                anchors.centerIn: parent
                width: dial.diameter - 10
                height: width
                rotation: -135 + knob.fraction * 270
                transformOrigin: Item.Center

                Rectangle {
                    width: 2
                    height: parent.height * 0.32
                    radius: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    color: Tokens.fg.default
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 5
                height: 5
                radius: 2
                color: Tokens.fg.default
            }
        }

        T.Label {
            width: parent.width
            height: 12
            text: Number(knob.liveValue).toFixed(knob.decimals) + knob.unit
            color: Tokens.fg.default
            font: Qt.font({ family: Tokens.type.monoFontFamily, pointSize: Tokens.type.caption })
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    T.ToolTip.visible: dragHandler.containsMouse && knob.enabledControl
    T.ToolTip.delay: 500
    T.ToolTip.text: qsTr("%1: drag to turn, double-click resets")
        .arg(knob.label)

    MouseArea {
        id: dragHandler

        anchors.fill: parent
        enabled: knob.enabledControl
        hoverEnabled: true
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: mouse => {
            knob._dragStartY = mouse.y
            knob._dragStartValue = knob.liveValue
            knob.forceActiveFocus(Qt.MouseFocusReason)
        }
        onPositionChanged: mouse => {
            if (!pressed) {
                return
            }
            // Vertical drag: one full turn (270°) per 120px of travel, so a
            // gate threshold and a makeup gain dial get the same feel.
            const travel = (knob._dragStartY - mouse.y) / 120.0
            knob.liveValue = knob.clamped(knob._dragStartValue + travel * knob.range)
        }
        onReleased: {
            if (knob.liveValue !== knob.value) {
                knob.committed(knob.liveValue)
            }
        }
        onDoubleClicked: knob.committed(knob.defaultValue)
        onWheel: wheel => {
            wheel.accepted = true
            knob.stepBy(wheel.angleDelta.y > 0 ? 1 : -1)
        }
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right) {
            knob.stepBy(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            knob.stepBy(-1)
            event.accepted = true
        }
    }
}
