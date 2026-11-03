// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The phone as the desktop's touchpad. Gestures are interpreted by the engine (TouchGestures.java): one finger moves,
// a tap clicks, press-and-hold drags, two fingers scroll, a two-finger tap right-clicks.
// AGENT-CONTRACT: Main.qml starts remote input while this page is shown (inputBegin/inputEnd). The surface forwards raw
// touch points through link.touch(); it never decides what a gesture means.
ScreenFrame {
    id: root
    title: qsTr("Touchpad")
    subtitle: qsTr("Move, tap, hold to drag, two fingers to scroll")
    backVisible: true

    readonly property bool live: link.inputState === "active"

    InputStateNotice { }

    Rectangle {
        id: pad
        objectName: "touchSurface"
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        implicitHeight: Math.max(240, root.height * 0.46)
        radius: Tk.Theme.radius.md
        color: Tk.Theme.color.panelAlt
        border.width: 1
        border.color: root.live ? Tk.Theme.color.accent : Tk.Theme.color.border
        opacity: root.live ? 1 : Tk.Theme.opacity.disabled
        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Touchpad surface")

        Tk.Caption {
            anchors.centerIn: parent
            text: root.live ? qsTr("Touch here") : qsTr("Waiting for the desktop")
            muted: true
        }

        MultiPointTouchArea {
            id: touch
            anchors.fill: parent
            enabled: root.live
            minimumTouchPoints: 1
            maximumTouchPoints: 2
            touchPoints: [ TouchPoint { id: tp0 }, TouchPoint { id: tp1 } ]
            property int down: 0

            // Forwards the whole touch frame; the timer repeats it so a held finger is noticed.
            function report() {
                const a = tp0.pressed, b = tp1.pressed
                const n = (a ? 1 : 0) + (b ? 1 : 0)
                const first = a ? tp0 : tp1
                const second = a && b ? tp1 : first
                down = n
                if (link.touch(n, first.x, first.y, second.x, second.y))
                    link.haptic()
            }
            onTouchUpdated: report()
            onReleased: report()
            onCanceled: { down = 0; link.touch(0, 0, 0, 0, 0) }

            Timer {
                interval: 50
                repeat: true
                running: touch.down > 0
                onTriggered: touch.report()
            }
        }
    }

    Tk.Flex {
        direction: Tk.Flex.Row
        gap: Tk.Theme.space.sm
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Button {
            objectName: "leftClick"
            text: qsTr("Left click")
            enabled: root.live
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            onClicked: { link.click("left"); link.haptic() }
        }
        Tk.Button {
            objectName: "rightClick"
            text: qsTr("Right click")
            enabled: root.live
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            onClicked: { link.click("right"); link.haptic() }
        }
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        Tk.Flex.alignSelf: Tk.Flex.Stretch
        Tk.Label { text: qsTr("Pointer speed") }
        Tk.Slider {
            objectName: "sensitivity"
            from: 0.5
            to: 3.5
            value: link.sensitivity
            decimals: 1
            showValue: true
            tooltip: qsTr("Pointer speed")
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            onValueModified: v => link.setSensitivity(v)
        }
    }
}
