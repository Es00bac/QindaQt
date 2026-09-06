// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0
import "DisplayArrangementGeometry.js" as Geometry

// Scaled diagram of every enabled display at its logical size. Dragging a
// tile proposes a snapped position; nothing reaches the model until the
// pointer is released, and even then it is only a draft for Apply.
Rectangle {
    id: canvas

    required property var outputs
    required property string selectedOutputId
    required property bool canEdit

    signal selectRequested(string stableId)
    signal moveRequested(string stableId, int x, int y)

    readonly property var rects: Geometry.enabledRects(canvas.outputs)
    // AGENT-GUARD: Tiles are keyed on this list, which only changes when the
    // set of enabled displays changes. Keying the Repeater on outputs or rects
    // would rebuild every tile on each draft edit and drop the keyboard focus
    // (and the pointer grab) of the display the user is moving.
    property var enabledIds: []
    readonly property int padding: Tokens.space["5"]
    readonly property real maxFit: 0.25
    readonly property int alignThresholdPx: 16
    property var frozenLayout: null
    readonly property var layout: canvas.frozenLayout !== null
                                  ? canvas.frozenLayout
                                  : Geometry.fitLayout(canvas.rects, canvas.width,
                                                       canvas.height, canvas.padding,
                                                       canvas.maxFit)
    property string draggingId: ""
    property var dragPreview: null
    readonly property bool dragging: canvas.draggingId.length > 0
    readonly property int tileCount: tileRepeater.count

    implicitHeight: 260
    radius: Tokens.radius.m
    color: Tokens.bg.highest
    border.width: Tokens.space["1"] / 2
    border.color: Tokens.outline.divider
    clip: true

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Display arrangement")
    Accessible.description: qsTr("%n display(s) shown at logical size. Drag a display or use its arrow keys to move it.", "", tileRepeater.count)

    function refreshEnabledIds() {
        const next = canvas.rects.map(rect => rect.stableId)
        if (next.join("\n") !== canvas.enabledIds.join("\n")) {
            canvas.enabledIds = next
        }
    }

    onRectsChanged: canvas.refreshEnabledIds()
    Component.onCompleted: canvas.refreshEnabledIds()

    function ordinalFor(stableId) {
        const list = canvas.outputs ?? []
        for (let index = 0; index < list.length; ++index) {
            if (list[index].stableId === stableId) {
                return index + 1
            }
        }
        return 0
    }

    function outputFor(stableId) {
        const list = canvas.outputs ?? []
        for (let index = 0; index < list.length; ++index) {
            if (list[index].stableId === stableId) {
                return list[index]
            }
        }
        return ({})
    }

    function rectFor(stableId) {
        return Geometry.findRect(canvas.rects, stableId)
    }

    function tileFor(stableId) {
        for (let index = 0; index < tileRepeater.count; ++index) {
            const item = tileRepeater.itemAt(index)
            if (item !== null && item.stableId === stableId) {
                return item
            }
        }
        return null
    }

    function beginDrag(stableId) {
        if (!canvas.canEdit || canvas.layout === null) {
            return
        }
        canvas.frozenLayout = canvas.layout
        canvas.draggingId = stableId
        canvas.dragPreview = null
    }

    function updateDrag(stableId, sceneDeltaX, sceneDeltaY) {
        const rect = canvas.rectFor(stableId)
        if (canvas.draggingId !== stableId || rect === null || canvas.layout === null) {
            return
        }
        const fit = canvas.layout.fit
        const desired = { x: rect.x + sceneDeltaX / fit, y: rect.y + sceneDeltaY / fit }
        canvas.dragPreview = Geometry.snapPosition(rect, canvas.rects, desired,
                                                   canvas.alignThresholdPx / fit)
    }

    function endDrag(stableId, moved) {
        const preview = canvas.dragPreview
        const rect = canvas.rectFor(stableId)
        const wasDragging = canvas.draggingId === stableId
        canvas.draggingId = ""
        canvas.dragPreview = null
        canvas.frozenLayout = null
        if (wasDragging && moved && preview !== null && rect !== null
                && (preview.x !== rect.x || preview.y !== rect.y)) {
            canvas.moveRequested(stableId, preview.x, preview.y)
        }
    }

    function cancelDrag() {
        canvas.draggingId = ""
        canvas.dragPreview = null
        canvas.frozenLayout = null
    }

    function nudge(stableId, deltaX, deltaY) {
        const rect = canvas.rectFor(stableId)
        if (!canvas.canEdit || rect === null) {
            return
        }
        const position = Geometry.snapPosition(rect, canvas.rects,
                                               { x: rect.x + deltaX, y: rect.y + deltaY }, 0)
        if (position.x !== rect.x || position.y !== rect.y) {
            canvas.moveRequested(stableId, position.x, position.y)
        }
    }

    function placeBeside(stableId, referenceId, side) {
        const rect = canvas.rectFor(stableId)
        const reference = canvas.rectFor(referenceId)
        if (!canvas.canEdit || rect === null || reference === null
                || stableId === referenceId || Geometry.SIDES.indexOf(side) < 0) {
            return
        }
        const position = Geometry.placeBeside(rect, reference, side, canvas.rects)
        if (position.x !== rect.x || position.y !== rect.y) {
            canvas.moveRequested(stableId, position.x, position.y)
        }
    }

    Text {
        anchors.centerIn: parent
        visible: tileRepeater.count === 0
        text: qsTr("No enabled displays to arrange.")
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.body
        color: Tokens.fg.muted
    }

    Repeater {
        id: tileRepeater
        model: canvas.enabledIds

        delegate: DisplayArrangementTile {
            id: tileDelegate
            required property string modelData

            readonly property var rect: Geometry.findRect(canvas.rects, tileDelegate.modelData)
            readonly property bool placed: tileDelegate.rect !== null && canvas.layout !== null
            readonly property bool previewing: canvas.dragging
                                               && canvas.draggingId === tileDelegate.modelData
                                               && canvas.dragPreview !== null
            readonly property int shownX: tileDelegate.previewing ? canvas.dragPreview.x
                                          : tileDelegate.placed ? tileDelegate.rect.x : 0
            readonly property int shownY: tileDelegate.previewing ? canvas.dragPreview.y
                                          : tileDelegate.placed ? tileDelegate.rect.y : 0
            readonly property var canvasPoint: tileDelegate.placed
                                               ? Geometry.toCanvas(canvas.layout,
                                                                   tileDelegate.shownX,
                                                                   tileDelegate.shownY)
                                               : ({ x: 0, y: 0 })

            objectName: "displayArrangementTile_" + tileDelegate.modelData
            visible: tileDelegate.placed
            output: canvas.outputFor(tileDelegate.modelData)
            ordinal: canvas.ordinalFor(tileDelegate.modelData)
            selected: canvas.selectedOutputId === tileDelegate.modelData
            draggable: canvas.canEdit
            dragging: tileDelegate.previewing
            logicalX: tileDelegate.shownX
            logicalY: tileDelegate.shownY
            logicalWidth: tileDelegate.placed ? tileDelegate.rect.width : 0
            logicalHeight: tileDelegate.placed ? tileDelegate.rect.height : 0
            x: tileDelegate.canvasPoint.x
            y: tileDelegate.canvasPoint.y
            width: tileDelegate.placed
                   ? Math.max(1, tileDelegate.rect.width * canvas.layout.fit) : 0
            height: tileDelegate.placed
                    ? Math.max(1, tileDelegate.rect.height * canvas.layout.fit) : 0
            z: tileDelegate.previewing ? 2 : (selected ? 1 : 0)

            onSelectRequested: canvas.selectRequested(tileDelegate.modelData)
            onDragStarted: canvas.beginDrag(tileDelegate.modelData)
            onDragMoved: (sceneDeltaX, sceneDeltaY) =>
                             canvas.updateDrag(tileDelegate.modelData, sceneDeltaX, sceneDeltaY)
            onDragFinished: moved => canvas.endDrag(tileDelegate.modelData, moved)
            onDragCanceled: canvas.cancelDrag()
            onNudgeRequested: (deltaX, deltaY) =>
                                  canvas.nudge(tileDelegate.modelData, deltaX, deltaY)
        }
    }
}
