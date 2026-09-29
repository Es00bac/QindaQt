// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0
import "TabletAreaGeometry.js" as Geometry

// One surface (the tablet as it lies on the desk, a screen, or the whole
// workspace) drawn to its real proportions, with the mapped rectangle on it.
// Drag inside the rectangle to move it, drag a corner to resize it, drag on
// the empty surface to draw a new one. Arrow keys move it, Shift with the
// arrow keys resizes it, and Home fills as much of the surface as the shape
// allows.
//
// AGENT-CONTRACT: the canvas never writes device state. It draws `area*`,
// previews a drag locally, and reports the finished rectangle through
// committed(); the model validates and writes, and the new `area*` flows
// back in. A refused rectangle therefore snaps back by itself.
Item {
    id: root

    // Width / height of the surface; anything unusable draws 16:10.
    property real surfaceAspect: 1.6
    // The committed rectangle, normalized to the surface.
    property real areaX: 0
    property real areaY: 0
    property real areaWidth: 1
    property real areaHeight: 1
    // Normalized width / height the rectangle keeps; 0 leaves it free.
    property real lockedAspect: 0
    property real minimumExtent: 0.05
    property real keyStep: 0.01
    // [{x, y, width, height, label}] outlines drawn inside the surface.
    property var outlines: []
    property string surfaceName: ""
    // Pixels around a corner that grab it for resizing.
    property real handleReach: 12

    signal committed(real x, real y, real width, real height)

    property bool dragging: false
    property string dragMode: ""
    property var draft: ({ x: 0, y: 0, width: 1, height: 1 })
    property var dragAnchor: ({ x: 0, y: 0 })
    property var grabOffset: ({ x: 0, y: 0 })

    readonly property real safeAspect: root.surfaceAspect > 0 && isFinite(root.surfaceAspect)
                                       ? root.surfaceAspect : 1.6
    readonly property var shown: root.dragging
                                 ? root.draft
                                 : ({ x: root.areaX, y: root.areaY,
                                      width: root.areaWidth, height: root.areaHeight })
    readonly property real frameWidth: Math.max(1, Math.min(root.width, root.height * root.safeAspect))
    readonly property real frameHeight: root.frameWidth / root.safeAspect

    implicitWidth: 260
    implicitHeight: 170
    activeFocusOnTab: true
    Accessible.role: Accessible.Canvas
    Accessible.focusable: true
    Accessible.name: root.surfaceName
    Accessible.description: qsTr("Mapped area from %1% to %2% across and from %3% to %4% down. Arrow keys move it, Shift with the arrow keys resizes it, and Home fills the surface.")
        .arg(Math.round(root.shown.x * 100))
        .arg(Math.round((root.shown.x + root.shown.width) * 100))
        .arg(Math.round(root.shown.y * 100))
        .arg(Math.round((root.shown.y + root.shown.height) * 100))

    function currentArea() {
        return { x: root.areaX, y: root.areaY, width: root.areaWidth, height: root.areaHeight }
    }

    function commit(rect) {
        if (!Geometry.same(rect, root.currentArea()))
            root.committed(rect.x, rect.y, rect.width, rect.height)
    }

    function normalizedPoint(pixelX, pixelY) {
        return {
            x: Geometry.clamp(pixelX / frame.width, 0, 1),
            y: Geometry.clamp(pixelY / frame.height, 0, 1)
        }
    }

    function beginDrag(pixelX, pixelY) {
        root.forceActiveFocus(Qt.MouseFocusReason)
        const point = root.normalizedPoint(pixelX, pixelY)
        const rect = root.currentArea()
        const anchor = Geometry.anchorOppositeCornerNear(
            rect, point, root.handleReach / frame.width, root.handleReach / frame.height)
        if (anchor !== null) {
            root.dragMode = "resize"
            root.dragAnchor = anchor
        } else if (Geometry.contains(rect, point)) {
            root.dragMode = "move"
            root.grabOffset = { x: point.x - rect.x, y: point.y - rect.y }
        } else {
            root.dragMode = "draw"
            root.dragAnchor = point
        }
        root.draft = rect
        root.dragging = true
    }

    function updateDrag(pixelX, pixelY) {
        if (!root.dragging)
            return
        const point = root.normalizedPoint(pixelX, pixelY)
        if (root.dragMode === "move") {
            const rect = root.currentArea()
            root.draft = Geometry.moved(rect, point.x - root.grabOffset.x - rect.x,
                                        point.y - root.grabOffset.y - rect.y)
            return
        }
        const next = Geometry.spanned(root.dragAnchor, point, root.lockedAspect)
        // A resize stops at the smallest usable size instead of collapsing; a
        // new rectangle may pass through small sizes while it is drawn.
        if (root.dragMode === "draw"
                || (next.width >= root.minimumExtent && next.height >= root.minimumExtent))
            root.draft = next
    }

    function endDrag() {
        if (!root.dragging)
            return
        const next = root.draft
        root.dragging = false
        // A click, or a drawn rectangle too small to use, changes nothing.
        if (next.width >= root.minimumExtent && next.height >= root.minimumExtent)
            root.commit(next)
    }

    Keys.onPressed: event => {
        const rect = root.currentArea()
        const resize = (event.modifiers & Qt.ShiftModifier) !== 0
        const step = root.keyStep
        let next = null
        if (event.key === Qt.Key_Home) {
            next = Geometry.largest(root.lockedAspect)
        } else if (event.key === Qt.Key_Left) {
            next = resize ? Geometry.resizedBy(rect, -step, 0, root.lockedAspect, root.minimumExtent)
                          : Geometry.moved(rect, -step, 0)
        } else if (event.key === Qt.Key_Right) {
            next = resize ? Geometry.resizedBy(rect, step, 0, root.lockedAspect, root.minimumExtent)
                          : Geometry.moved(rect, step, 0)
        } else if (event.key === Qt.Key_Up) {
            next = resize ? Geometry.resizedBy(rect, 0, -step, root.lockedAspect, root.minimumExtent)
                          : Geometry.moved(rect, 0, -step)
        } else if (event.key === Qt.Key_Down) {
            next = resize ? Geometry.resizedBy(rect, 0, step, root.lockedAspect, root.minimumExtent)
                          : Geometry.moved(rect, 0, step)
        } else {
            return
        }
        event.accepted = true
        root.commit(next)
    }

    Rectangle {
        id: frame
        objectName: "tabletAreaFrame"
        anchors.centerIn: parent
        width: root.frameWidth
        height: root.frameHeight
        radius: Tokens.radius.s
        color: Tokens.bg.sunken
        border.color: root.activeFocus ? Tokens.focus.ring : Tokens.outline.divider
        border.width: root.activeFocus ? Tokens.space["1"] : 1
        Accessible.ignored: true

        // Each screen of a workspace, so "a specific space" is recognizable.
        Repeater {
            model: root.outlines

            delegate: Rectangle {
                id: outline
                required property var modelData
                x: frame.width * outline.modelData.x
                y: frame.height * outline.modelData.y
                width: frame.width * outline.modelData.width
                height: frame.height * outline.modelData.height
                color: "transparent"
                border.color: Tokens.outline.strong
                border.width: 1
                Accessible.ignored: true

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: Tokens.space["1"]
                    width: Math.max(0, parent.width - 2 * Tokens.space["1"])
                    text: String(outline.modelData.label ?? "")
                    elide: Text.ElideRight
                    color: Tokens.fg.muted
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    Accessible.ignored: true
                }
            }
        }

        Rectangle {
            id: areaRect
            objectName: "tabletAreaRectangle"
            x: frame.width * root.shown.x
            y: frame.height * root.shown.y
            width: frame.width * root.shown.width
            height: frame.height * root.shown.height
            radius: Tokens.radius.s
            color: Tokens.accent.subtle
            border.color: Tokens.accent.default
            border.width: 2
            Accessible.ignored: true

            // Corner handles: resizing is a corner drag.
            Repeater {
                model: [ { x: 0, y: 0 }, { x: 1, y: 0 }, { x: 0, y: 1 }, { x: 1, y: 1 } ]

                delegate: Rectangle {
                    id: handle
                    required property var modelData
                    width: 10
                    height: 10
                    radius: 5
                    x: areaRect.width * handle.modelData.x - width / 2
                    y: areaRect.height * handle.modelData.y - height / 2
                    color: Tokens.accent.default
                    border.color: Tokens.bg.base
                    border.width: 1
                    Accessible.ignored: true
                }
            }
        }

        MouseArea {
            objectName: "tabletAreaPointer"
            anchors.fill: parent
            preventStealing: true
            onPressed: mouse => root.beginDrag(mouse.x, mouse.y)
            onPositionChanged: mouse => root.updateDrag(mouse.x, mouse.y)
            onReleased: root.endDrag()
            onCanceled: root.dragging = false
        }
    }
}
