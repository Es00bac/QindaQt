// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0
import "DisplayArrangementGeometry.js" as Geometry

// One display in the arrangement diagram. The tile is a focusable, numbered
// stand-in for an output: clicking selects it, dragging (when draggable)
// proposes a new position, and arrow keys slide it along the edge it touches.
// The parent canvas owns geometry; the tile only reports intent.
Item {
    id: tile

    required property var output
    required property int ordinal
    required property bool selected
    required property bool draggable
    property bool inactive: false
    property bool dragging: false
    property int logicalX: 0
    property int logicalY: 0
    property int logicalWidth: 0
    property int logicalHeight: 0

    signal selectRequested()
    signal dragStarted()
    signal dragMoved(real sceneDeltaX, real sceneDeltaY)
    signal dragFinished(bool moved)
    signal dragCanceled()
    signal nudgeRequested(int deltaX, int deltaY)

    readonly property string stableId: String(tile.output.stableId ?? "")
    readonly property string displayLabel: String(tile.output.label ?? tile.output.connectorName ?? "")
    readonly property bool primary: tile.output.primary ?? false
    readonly property int dragThreshold: 3
    readonly property bool compact: width < 120 || height < 64

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.focusable: true
    Accessible.name: tile.inactive
                     ? qsTr("Display %1, %2, off").arg(tile.ordinal).arg(tile.displayLabel)
                     : qsTr("Display %1, %2%3").arg(tile.ordinal).arg(tile.displayLabel)
                       .arg(tile.primary ? qsTr(", primary") : "")
    Accessible.description: tile.inactive
                            ? qsTr("Select it, then turn on Enable display to place it.")
                            : qsTr("Position %1, %2. Logical size %3 by %4.%5")
                              .arg(tile.logicalX).arg(tile.logicalY)
                              .arg(tile.logicalWidth).arg(tile.logicalHeight)
                              .arg(tile.draggable
                                   ? qsTr(" Arrow keys slide it along the edge it touches; Shift moves further.")
                                   : "")
    Accessible.onPressAction: tile.selectRequested()

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter) {
            tile.selectRequested()
            event.accepted = true
            return
        }
        if (!tile.draggable) {
            return
        }
        const step = (event.modifiers & Qt.ShiftModifier)
                   ? Geometry.NUDGE_STEP_LARGE : Geometry.NUDGE_STEP
        if (event.key === Qt.Key_Left) {
            tile.nudgeRequested(-step, 0)
        } else if (event.key === Qt.Key_Right) {
            tile.nudgeRequested(step, 0)
        } else if (event.key === Qt.Key_Up) {
            tile.nudgeRequested(0, -step)
        } else if (event.key === Qt.Key_Down) {
            tile.nudgeRequested(0, step)
        } else {
            return
        }
        event.accepted = true
    }

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Tokens.radius.s
        color: tile.inactive ? "transparent"
             : tile.selected ? Tokens.accent.subtle : Tokens.bg.raised
        border.width: tile.selected || tile.activeFocus ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: tile.activeFocus ? Tokens.focus.ring
                    : tile.selected ? Tokens.accent.default : Tokens.outline.strong
        opacity: tile.inactive ? 0.7 : 1.0
        Accessible.ignored: true

        // Primary display marker: the amber rule where its panel would sit.
        Rectangle {
            visible: tile.primary && !tile.inactive
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Tokens.space["1"]
            height: Tokens.space["1"]
            radius: height / 2
            color: Tokens.accent.default
            Accessible.ignored: true
        }

        Rectangle {
            id: badge
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Tokens.space["2"]
            width: Math.max(22, badgeText.implicitWidth + Tokens.space["2"])
            height: 22
            radius: height / 2
            color: tile.inactive ? Tokens.bg.raised : Tokens.accent.default
            border.width: tile.inactive ? Tokens.space["1"] / 2 : 0
            border.color: Tokens.outline.strong
            Accessible.ignored: true

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: tile.ordinal
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                font.weight: Font.DemiBold
                color: tile.inactive ? Tokens.fg.muted : Tokens.accent.fg
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Tokens.space["2"]
            spacing: 0

            Text {
                width: parent.width
                text: tile.displayLabel
                elide: Text.ElideRight
                maximumLineCount: 1
                font.family: Tokens.type.fontFamily
                font.pointSize: tile.compact ? Tokens.type.caption : Tokens.type.body
                font.weight: Font.DemiBold
                color: tile.inactive ? Tokens.fg.muted : Tokens.fg.default
            }

            Text {
                width: parent.width
                visible: !tile.inactive && !tile.compact
                text: qsTr("%1 × %2").arg(tile.logicalWidth).arg(tile.logicalHeight)
                elide: Text.ElideRight
                maximumLineCount: 1
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                color: Tokens.fg.muted
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: tile.dragging ? Tokens.state.pressed
                 : pointerArea.containsMouse && !tile.inactive ? Tokens.state.hover
                 : "transparent"
            Accessible.ignored: true
        }
    }

    MouseArea {
        id: pointerArea
        anchors.fill: parent
        hoverEnabled: true
        // AGENT-GUARD: The page scrolls inside a Flickable. Without
        // preventStealing a vertical drag hands the grab to the Flickable and
        // the display jumps back while the form scrolls instead.
        preventStealing: true
        cursorShape: !tile.draggable ? Qt.ArrowCursor
                   : pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        property real pressSceneX: 0
        property real pressSceneY: 0
        property bool moved: false

        onPressed: mouse => {
            tile.forceActiveFocus(Qt.MouseFocusReason)
            tile.selectRequested()
            const scene = pointerArea.mapToItem(null, mouse.x, mouse.y)
            pressSceneX = scene.x
            pressSceneY = scene.y
            moved = false
            if (tile.draggable) {
                tile.dragStarted()
            }
        }
        onPositionChanged: mouse => {
            if (!pressed || !tile.draggable) {
                return
            }
            const scene = pointerArea.mapToItem(null, mouse.x, mouse.y)
            const deltaX = scene.x - pressSceneX
            const deltaY = scene.y - pressSceneY
            if (!moved && Math.abs(deltaX) < tile.dragThreshold
                    && Math.abs(deltaY) < tile.dragThreshold) {
                return
            }
            moved = true
            tile.dragMoved(deltaX, deltaY)
        }
        onReleased: {
            if (tile.draggable) {
                tile.dragFinished(moved)
            }
            moved = false
        }
        onCanceled: {
            if (tile.draggable) {
                tile.dragCanceled()
            }
            moved = false
        }
    }
}
