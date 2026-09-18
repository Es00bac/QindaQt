// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Pressure: the curve KWin applies between pen force and reported pressure,
// the tip threshold, and a place to try it.
//
// AGENT-NOTE: KWin reads exactly two control points of a cubic bezier from
// (0,0) to (1,1) and serializes them as "x,y;x,y;" (kwin 6.6.6
// src/backends/libinput/device.cpp serializePressureCurve). The two sliders
// below ARE those control points; a third point would be dropped silently.
ColumnLayout {
    id: root

    required property var selection

    readonly property var curvePoints: root.parsePoints(
        root.selection !== null ? root.selection.pressureCurve : "")
    readonly property real firstX: root.curvePoints.length > 0 ? root.curvePoints[0].x : 0
    readonly property real firstY: root.curvePoints.length > 0 ? root.curvePoints[0].y : 0
    readonly property real secondX: root.curvePoints.length > 1 ? root.curvePoints[1].x : 1
    readonly property real secondY: root.curvePoints.length > 1 ? root.curvePoints[1].y : 1

    function parsePoints(curve) {
        const points = []
        const pairs = String(curve).split(";")
        for (let index = 0; index < pairs.length; ++index) {
            const parts = pairs[index].split(",")
            if (parts.length !== 2)
                continue
            const x = Number(parts[0])
            const y = Number(parts[1])
            if (isFinite(x) && isFinite(y))
                points.push({ x: x, y: y })
        }
        return points
    }

    function writeCurve(x1, y1, x2, y2) {
        const clamp = value => Math.max(0, Math.min(1, value))
        // x must strictly increase or KWin keeps its old curve and the route
        // would show an unobserved change as applied.
        const left = clamp(x1)
        const right = Math.max(left + 0.01, clamp(x2))
        root.selection.applyPressureCurve(
            left.toFixed(3) + "," + clamp(y1).toFixed(3) + ";"
            + right.toFixed(3) + "," + clamp(y2).toFixed(3) + ";")
    }

    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Pressure")
        description: qsTr("How hard the pen has to press for a given line weight")
    }

    // The curve, drawn from the two control points KWin stores.
    Canvas {
        id: curveCanvas
        objectName: "tabletPressureCurve"
        Layout.preferredWidth: 220
        Layout.preferredHeight: 140
        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("Pressure curve")
        Accessible.description: qsTr("Control points at %1, %2 and %3, %4")
            .arg(root.firstX.toFixed(2)).arg(root.firstY.toFixed(2))
            .arg(root.secondX.toFixed(2)).arg(root.secondY.toFixed(2))

        readonly property color lineColor: Tokens.accent.default
        readonly property color frameColor: Tokens.outline.divider
        readonly property color groundColor: Tokens.bg.sunken

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.fillStyle = curveCanvas.groundColor
            context.fillRect(0, 0, width, height)
            context.strokeStyle = curveCanvas.frameColor
            context.lineWidth = 1
            context.strokeRect(0.5, 0.5, width - 1, height - 1)
            context.strokeStyle = curveCanvas.lineColor
            context.lineWidth = 2
            context.beginPath()
            context.moveTo(0, height)
            context.bezierCurveTo(root.firstX * width, height - root.firstY * height,
                                  root.secondX * width, height - root.secondY * height,
                                  width, 0)
            context.stroke()
        }

        Connections {
            target: root
            function onCurvePointsChanged() { curveCanvas.requestPaint() }
        }
    }

    FormRow {
        objectName: "tabletPressureSoftRow"
        Layout.fillWidth: true
        label: qsTr("Soft end")
        description: qsTr("Lower gives a lighter touch at the start of the stroke")
        editor: Slider {
            objectName: "tabletPressureSoftSlider"
            from: 0.0
            to: 1.0
            stepSize: 0.01
            value: root.firstY
            accessibleName: qsTr("Soft end of the pressure curve")
            onMoved: root.writeCurve(root.firstX, value, root.secondX, root.secondY)
        }
    }

    FormRow {
        objectName: "tabletPressureFirmRow"
        Layout.fillWidth: true
        label: qsTr("Firm end")
        description: qsTr("Lower needs more force before the line reaches full weight")
        editor: Slider {
            objectName: "tabletPressureFirmSlider"
            from: 0.0
            to: 1.0
            stepSize: 0.01
            value: root.secondY
            accessibleName: qsTr("Firm end of the pressure curve")
            onMoved: root.writeCurve(root.firstX, root.firstY, root.secondX, value)
        }
    }

    FormRow {
        objectName: "tabletPressureThresholdRow"
        Layout.fillWidth: true
        visible: root.selection !== null && root.selection.pressureRangeAvailable
        label: qsTr("Tip threshold")
        description: qsTr("How much force counts as touching the surface at all")
        editor: Slider {
            objectName: "tabletPressureThresholdSlider"
            from: 0.0
            to: 0.9
            stepSize: 0.01
            value: root.selection !== null ? root.selection.pressureRangeMin : 0.0
            accessibleName: qsTr("Tip threshold")
            onMoved: root.selection.pressureRangeMin = value
        }
    }

    // Try it here rather than opening a drawing application to find out.
    Label {
        Layout.fillWidth: true
        muted: true
        text: qsTr("Scribble below to try the curve.")
    }

    TabletPressureScribble {
        objectName: "tabletPressureScribble"
        Layout.fillWidth: true
        Layout.preferredHeight: 120
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Button {
            objectName: "tabletPressureReset"
            text: qsTr("Reset pressure")
            emphasized: false
            accessibleDescription: qsTr("Restore the pressure curve and threshold this tablet ships with")
            onClicked: root.selection.resetPressure()
        }
    }
}
