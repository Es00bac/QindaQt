// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Controls 1.0 as C

// One desktop icon. Extracted from DesktopIconsView so the view keeps its
// placement and selection logic readable and so the source-shape gate does not
// read a large inline delegate as one enormous function.
//
// AGENT-CONTRACT: the tile owns no placement. Its x/y are bound to the view's
// resolved local position for this entry, and a drag reports pointer motion
// back to the view, which republishes positions through the shared layout
// store. Never assign x/y here: on a cross-output drag the surface that draws
// this tile may not be the surface holding the pointer grab, and an imperative
// assignment would fight the binding that keeps the two in step.
Rectangle {
    id: tile

    required property var view
    required property var modelData
    required property int index

    readonly property string entryId: String(modelData.id)
    readonly property string entryLabel: String(modelData.label)
    readonly property string layoutKey: String(modelData.layoutKey)
    readonly property bool selected: view.isSelected(entryId)
    // True only on the surface that owns the pointer grab for this drag.
    property bool dragged: false

    objectName: "desktopIconsTile"
    width: view.tileWidth
    height: view.tileHeight
    radius: 4
    color: selected ? "#33ffffff"
          : tileInput.containsMouse ? "#22ffffff" : "transparent"

    x: view.placement.localX(layoutKey)
    y: view.placement.localY(layoutKey)
    // This output owns the icon, or this surface is dragging it. A hidden tile
    // still exists so the Repeater model stays stable (see the view's guard).
    visible: view.drawsRow(layoutKey)
    enabled: visible

    // AGENT-NOTE: animation is enabled only while this tile is NOT the one the
    // pointer is dragging. A dragged icon must track the pointer exactly (the
    // user's "does not move smoothly" complaint was a drag that lagged and
    // jumped); every other movement - arrange, restore, an icon arriving from
    // another output, a layout change - should glide instead of teleport.
    // AGENT-NOTE: animation is disabled for an icon the pointer is dragging, so
    // it tracks the pointer exactly (the user's "does not move smoothly"
    // complaint was a drag that lagged and jumped). Every other movement -
    // arrange, a snap settling after a drop, an icon arriving from another
    // output - glides instead of teleporting.
    Behavior on x {
        enabled: tile.view.placement.animationsLive
                 && !tile.view.isDragKey(tile.layoutKey)
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }
    Behavior on y {
        enabled: tile.view.placement.animationsLive
                 && !tile.view.isDragKey(tile.layoutKey)
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    Keys.onReturnPressed: tile.view.openEntry(tile.entryId)
    Keys.onEnterPressed: tile.view.openEntry(tile.entryId)

    MouseArea {
        id: tileInput
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor

        // Press point in the VIEW's coordinate space, which does not move with
        // the tile. Measuring in tile coordinates made every move report
        // (delta2 - delta1) instead of delta2, because assigning tile.x moved
        // this MouseArea's own origin under the pointer.
        property real pressViewX: 0
        property real pressViewY: 0
        property bool moved: false

        onPressed: (mouse) => {
            tile.dragged = false
            moved = false
            if (mouse.button === Qt.LeftButton) {
                if (mouse.modifiers & Qt.ControlModifier) {
                    tile.view.selection.toggle(tile.entryId)
                } else if (mouse.modifiers & Qt.ShiftModifier) {
                    tile.view.selection.rangeTo(tile.entryId)
                } else if (!tile.view.isSelected(tile.entryId)) {
                    tile.view.selection.selectOnly(tile.entryId)
                }
                const point = tileInput.mapToItem(tile.view, mouse.x, mouse.y)
                pressViewX = point.x
                pressViewY = point.y
                tile.view.beginDrag(tile)
                tile.forceActiveFocus(Qt.MouseFocusReason)
            } else if (mouse.button === Qt.RightButton
                       && !tile.view.isSelected(tile.entryId)) {
                tile.view.selection.selectOnly(tile.entryId)
            }
            // AGENT-GUARD: Qt.MiddleButton stays claimed as a no-op so a middle
            // click over a tile never falls through to the surface input's
            // Applications popup.
        }
        onPositionChanged: (mouse) => {
            if (!pressed || !tile.view.dragInFlight)
                return
            const point = tileInput.mapToItem(tile.view, mouse.x, mouse.y)
            const dx = point.x - pressViewX
            const dy = point.y - pressViewY
            if (!moved && Math.abs(dx) + Math.abs(dy) < 3)
                return
            moved = true
            tile.dragged = true
            tile.view.moveDrag(dx, dy)
        }
        onReleased: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            if (tile.dragged) {
                tile.view.commitDrag()
            } else {
                tile.view.cancelDrag(tile, mouse.modifiers)
            }
            tile.dragged = false
        }
        onCanceled: {
            tile.view.cancelDrag(tile, Qt.NoModifier)
            tile.dragged = false
        }
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                tile.view.openIconMenu(tile, mouse.x, mouse.y)
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton && !tile.dragged)
                tile.view.openEntry(tile.entryId)
        }
    }

    C.TouchContextArea {
        objectName: "desktopIconTouchContext"
        anchors.fill: parent
        onContextRequested: (position) => {
            if (!tile.view.isSelected(tile.entryId))
                tile.view.selection.selectOnly(tile.entryId)
            tile.view.openIconMenu(tile, position.x, position.y)
        }
    }

    ShellIcons.Icon {
        objectName: "desktopIconsTileIcon"
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        name: String(tile.modelData.iconName) || "folder"
        size: tile.view.iconSize
        fallbackText: tile.entryLabel
        Accessible.ignored: true
    }
    Text {
        objectName: "desktopIconsTileLabel"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 8
        text: tile.entryLabel
        color: "#ffffff"
        style: Text.Raised
        styleColor: "#80000000"
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideMiddle
        font.pixelSize: 12
        Accessible.ignored: true
    }
    Accessible.role: Accessible.Button
    Accessible.name: String(tile.modelData.accessibleName)
    Accessible.selected: tile.selected
    Accessible.onPressAction: tile.view.openEntry(tile.entryId)
}
