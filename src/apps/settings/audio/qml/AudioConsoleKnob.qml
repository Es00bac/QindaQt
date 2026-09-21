// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk

// A rotary rack knob: drag vertically to turn, double-click restores the
// block default, and the value commits ONCE on release so a drag is one
// operation rather than a stream of them (the rack is judged whole by the
// service; see ADR-0179).
//
// AGENT-NOTE: belongs in QindaTK as a RotaryKnob; the toolkit has no rotary
// control. Kept local because the QindaTK repository must not be changed from
// this lane. The dial is drawn from plain items — an LED tick ring lit up to
// the value plus a rotating needle — rather than Canvas: Canvas content does
// not paint under offscreen capture (the page-test grab path), and the tick
// ring reads better at desk size anyway.
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
    // The rack shows the label row; a strip's pan slot prints its own caption
    // and turns this off to stay at desk density.
    property bool showLabel: true
    // Custom text for the value row, e.g. pan's L/C/R. Signature: (value) => string.
    property var formatValue: null

    signal committed(real value)

    readonly property real labelHeight: showLabel ? 12 : 0
    implicitWidth: 52
    implicitHeight: labelHeight + dial.diameter + 14
    activeFocusOnTab: enabledControl

    readonly property real range: knob.to - knob.from
    readonly property real fraction: knob.range > 0
        ? Math.max(0.0, Math.min(1.0, (knob.liveValue - knob.from) / knob.range))
        : 0.0
    readonly property string valueText: knob.formatValue !== null
        ? knob.formatValue(knob.liveValue)
        : Number(knob.liveValue).toFixed(knob.decimals) + knob.unit

    // The drag edits this, not `value`: the projected binding must stay
    // intact so a model republish during a drag cannot fight the pointer.
    property real liveValue: knob.value
    property real _dragStartValue: 0.0
    property real _dragStartY: 0.0

    opacity: knob.enabledControl ? 1.0 : Tk.Theme.opacity.disabled
    Accessible.role: Accessible.Slider
    Accessible.name: knob.label
    Accessible.description: qsTr("%1 %2").arg(knob.valueText).arg(knob.unit)
    Accessible.onIncreaseAction: if (knob.enabledControl) knob.stepBy(1)
    Accessible.onDecreaseAction: if (knob.enabledControl) knob.stepBy(-1)

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
            height: knob.labelHeight
            visible: knob.showLabel
            text: knob.label
            color: Tk.Theme.color.textMuted
            font.pixelSize: Tk.Theme.font.caption
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        // The dial. Tick ring first (lit ticks are the value readout), then
        // the well, then the needle on top.
        Item {
            id: dial

            readonly property real diameter: Math.min(knob.width, knob.implicitHeight - knob.labelHeight - 14)

            width: parent.width
            height: diameter

            Repeater {
                model: 21
                delegate: Rectangle {
                    id: tick
                    required property int index
                    readonly property real tickAngle: -135 + tick.index * 270 / 20
                    readonly property bool lit: tick.index / 20 <= knob.fraction + 0.001
                    width: 2
                    height: tickAngle % 90 === 0 ? 5 : 3
                    radius: 1
                    x: dial.width / 2 - 1
                    y: dial.height / 2 - dial.diameter / 2 - 1
                    transform: Rotation {
                        origin.x: 1
                        origin.y: dial.diameter / 2 + 1
                        angle: tick.tickAngle
                    }
                    color: lit ? Tk.Theme.color.accent : Tk.Theme.color.divider
                    opacity: lit ? 1.0 : 0.6
                }
            }

            // The value halo: a ring that brightens as the dial moves away
            // from its block default, so a rack that has been touched reads as
            // touched at a glance without having to compare numbers.
            Rectangle {
                anchors.centerIn: parent
                width: dial.diameter - 4
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: Tk.Theme.color.accent
                visible: knob.range > 0
                opacity: 0.55 * Math.min(1.0,
                    Math.abs(knob.liveValue - knob.defaultValue) / (knob.range / 2))
            }

            Rectangle {
                anchors.centerIn: parent
                width: dial.diameter - 10
                height: width
                radius: width / 2
                color: Tk.Theme.color.canvas
                border.width: Tk.Theme.size.border
                border.color: knob.activeFocus ? Tk.Theme.color.accent : Tk.Theme.color.borderStrong
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
                    color: Tk.Theme.color.text
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 5
                height: 5
                radius: 2
                color: Tk.Theme.color.text
            }
        }

        T.Label {
            width: parent.width
            height: 12
            text: knob.valueText
            color: Tk.Theme.color.text
            font.pixelSize: Tk.Theme.font.caption
            font.family: Tk.Theme.font.monoFamily
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
