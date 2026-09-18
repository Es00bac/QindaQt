// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Four-target calibration, on the tablet's own screen.
//
// AGENT-GUARD: this is a Window, not an overlay inside the Settings page, and
// it is placed on the OUTPUT THE PEN IS MAPPED TO. Calibrating inside the
// settings window measures wherever that window happens to be — on a
// multi-head desktop that is usually not the pen display, so every measured
// point is meaningless and the resulting matrix makes the pen worse.
//
// AGENT-GUARD: it measures the STYLUS, not the cursor. A MouseArea accepts
// whatever moved the pointer last, so a stray touchpad tap would be recorded
// as a pen sample and skew the fit.
//
// AGENT-CONTRACT: the caller resets the calibration to the device default
// before showing this, so what the pen reports here is uncalibrated.
// Measuring through an existing matrix would compose two corrections and
// drift further every pass.
Window {
    id: root

    required property var selection
    // The connector name the pen is mapped to, or empty for "wherever the
    // pointer is". The wizard refuses to run in that case rather than
    // calibrate against an unknown screen.
    required property string targetOutput
    signal finished(bool applied)

    readonly property var targetScreen: {
        const screens = Qt.application.screens
        for (let index = 0; index < screens.length; ++index) {
            if (screens[index].name === root.targetOutput)
                return screens[index]
        }
        return null
    }

    // Normalized target positions, inset from the corners so a target is
    // never under a screen edge or a panel.
    readonly property var targets: [
        { x: 0.1, y: 0.1 }, { x: 0.9, y: 0.1 },
        { x: 0.9, y: 0.9 }, { x: 0.1, y: 0.9 }
    ]
    property int currentTarget: 0
    property var measured: []

    objectName: "tabletCalibrationOverlay"
    screen: root.targetScreen !== null ? root.targetScreen : Qt.application.screens[0]
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visibility: Window.FullScreen
    color: Tokens.bg.sunken
    title: qsTr("Calibrate the pen display")

    function restart() {
        root.measured = []
        root.currentTarget = 0
    }

    function recordPoint(normalizedX, normalizedY) {
        const points = root.measured.slice()
        points.push({ x: normalizedX, y: normalizedY })
        root.measured = points
        if (points.length < root.targets.length) {
            root.currentTarget = points.length
            return
        }
        const matrix = root.selection.calibrationMatrixFor(points, root.targets)
        const applied = root.selection.applyCalibrationMatrix(matrix)
        if (!applied)
            root.restart()
        root.finished(applied)
    }

    Item {
        id: surface
        objectName: "tabletCalibrationCapture"
        anchors.fill: parent

        // AGENT-GUARD: stylus only. Accepting the mouse here would let a
        // touchpad tap become a calibration sample.
        PointHandler {
            id: stylus
            acceptedDevices: PointerDevice.Stylus
            onActiveChanged: {
                if (!active && point.position.x >= 0)
                    root.recordPoint(point.position.x / Math.max(1, surface.width),
                                     point.position.y / Math.max(1, surface.height))
            }
        }

        Repeater {
            model: root.targets
            delegate: Item {
                required property int index
                required property var modelData
                objectName: "tabletCalibrationTarget_" + index
                x: surface.width * modelData.x - width / 2
                y: surface.height * modelData.y - height / 2
                width: 44
                height: 44
                opacity: index === root.currentTarget ? 1.0 : 0.28

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: 2
                    color: Tokens.accent.default
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 2
                    height: parent.height
                    color: Tokens.accent.default
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    radius: 7
                    color: "transparent"
                    border.color: Tokens.accent.default
                    border.width: 2
                }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: Tokens.space["3"]
            width: Math.min(parent.width - Tokens.space["6"] * 2, 420)

            Label {
                objectName: "tabletCalibrationInstruction"
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Touch the highlighted crosshair with the pen (%1 of %2).")
                    .arg(root.currentTarget + 1).arg(root.targets.length)
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            Label {
                objectName: "tabletCalibrationScreenNote"
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                muted: true
                text: root.targetScreen !== null
                      ? qsTr("Calibrating on %1.").arg(root.targetOutput)
                      : qsTr("This tablet is not mapped to a screen, so there is nothing to calibrate against. Choose a screen under Map to first.")
            }

            Button {
                objectName: "tabletCalibrationCancel"
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Cancel")
                accessibleDescription: qsTr("Leave the calibration unchanged")
                onClicked: {
                    root.restart()
                    root.finished(false)
                }
            }
        }
    }

    onActiveChanged: {
        // Escape must work even though the window has no decoration.
        if (active)
            surface.forceActiveFocus()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: {
            root.restart()
            root.finished(false)
        }
    }
}
