// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// A place to try the pen. Stroke width follows the reported pressure, so the
// curve above is visible in the line rather than described in words.
//
// AGENT-NOTE: Qt delivers tablet pressure through QTabletEvent, which QML
// does not expose; a plain pointer stroke carries no pressure, so this draws
// with the pressure the handler reports and falls back to a constant width
// for a mouse. It is a feel check, not a measurement.
Canvas {
    id: root

    property var strokes: []
    property var currentStroke: []

    objectName: "tabletScribbleCanvas"
    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Pen test area")
    Accessible.description: qsTr("Draw here to see how the pressure settings feel")

    readonly property color inkColor: Tokens.fg.default
    readonly property color groundColor: Tokens.bg.sunken
    readonly property color frameColor: Tokens.outline.divider

    function clearStrokes() {
        root.strokes = []
        root.currentStroke = []
        requestPaint()
    }

    onPaint: {
        const context = getContext("2d")
        context.reset()
        context.fillStyle = root.groundColor
        context.fillRect(0, 0, width, height)
        context.strokeStyle = root.frameColor
        context.lineWidth = 1
        context.strokeRect(0.5, 0.5, width - 1, height - 1)
        context.strokeStyle = root.inkColor
        context.lineCap = "round"
        const all = root.strokes.concat([root.currentStroke])
        for (let s = 0; s < all.length; ++s) {
            const stroke = all[s]
            for (let index = 1; index < stroke.length; ++index) {
                context.lineWidth = 1 + stroke[index].pressure * 7
                context.beginPath()
                context.moveTo(stroke[index - 1].x, stroke[index - 1].y)
                context.lineTo(stroke[index].x, stroke[index].y)
                context.stroke()
            }
        }
    }

    PointHandler {
        id: pointHandler
        acceptedDevices: PointerDevice.Stylus | PointerDevice.Mouse
                         | PointerDevice.TouchScreen
        onActiveChanged: {
            if (active) {
                root.currentStroke = []
                return
            }
            if (root.currentStroke.length > 1)
                root.strokes = root.strokes.concat([root.currentStroke])
            root.currentStroke = []
            root.requestPaint()
        }
        onPointChanged: {
            if (!active)
                return
            const stroke = root.currentStroke.slice()
            stroke.push({
                x: point.position.x,
                y: point.position.y,
                pressure: point.pressure > 0 ? point.pressure : 0.5
            })
            root.currentStroke = stroke
            root.requestPaint()
        }
    }

    TapHandler {
        objectName: "tabletScribbleClear"
        acceptedButtons: Qt.RightButton
        onTapped: root.clearStrokes()
    }
}
