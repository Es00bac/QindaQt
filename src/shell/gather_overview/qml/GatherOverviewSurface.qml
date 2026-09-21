// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The gather overview's drawing surface. It draws exactly what
// `projectGatherOverview()` handed it and decides nothing: every frame comes
// from the projection, and the projection's frames come from the planner.
//
// AGENT-CONTRACT: `projection` is the value object the model produces, with
// `lane` as one of the strings "icon", "card" or "window". A string rather
// than an enum ordinal on purpose - the surface must not break if the C++
// enum is reordered, and a stub in a test should read as what it means.
//
// The surface owns no scroll state. A wheel event becomes `scrollRequested`
// and the owner adds it to the offset and re-projects, because the planner is
// the only thing allowed to clamp a scroll position. Same reasoning for
// activation: the surface reports which item was clicked and never acts.
//
// AGENT-CONTRACT: all visuals derive from semantic QST-1 roles
// (QindaQt.Tokens). No colours, sizes, fonts or durations are written here.
// The scrim wears the popup material because that is what this is - a
// transient overlay - so a theme's transparency choice reaches it without a
// number being invented for the scrim alone.
Item {
    id: root

    // The projection to draw.
    required property var projection
    // The desktop-logical point this item's (0,0) is at. Projection frames are
    // desktop-logical, so a frame is drawn at `frame - origin`; a surface that
    // exactly covers the work area of an output at (0,0) can leave this alone.
    property point origin: Qt.point(0, 0)

    readonly property bool ready: Tokens.ready && projection !== null
                                 && projection !== undefined
    readonly property bool available: ready && Boolean(projection.available)
    // A Degraded source still draws - it is the last thing the user actually
    // saw - but nothing in it can be acted on.
    readonly property bool interactive: available
                                        && Boolean(projection.interactive)
    readonly property bool showsEmptyState: available
                                             && Boolean(projection.empty)
    readonly property bool reducedMotion: Tokens.ready
                                           && Tokens.accessibility.reducedMotion

    // Guarded reads, so a binding never dereferences an absent projection.
    // Bindings evaluate whether or not the item using them is visible.
    readonly property rect fieldRect: available ? projection.field
                                                : Qt.rect(0, 0, 0, 0)
    readonly property int hiddenWindows: available ? projection.windowsHidden : 0
    readonly property int sourceOverflow: available
                                          ? projection.sourceOverflowCount : 0
    readonly property int gridColumns: available ? projection.gridColumns : 0
    readonly property real gridRowExtent: available && projection.gridRows > 0
                                          ? projection.gridContentHeight
                                            / projection.gridRows
                                          : 0

    // The item the user chose. Carries taskId, windowId and the
    // generationRevision it was projected from, so the owner can echo the
    // generation and let stale-revision arbitration refuse a dead action.
    signal activated(var item)
    // Wheel delta in pixels, positive scrolling further down the grid.
    signal scrollRequested(real delta)
    // Escape, or a click that missed every tile.
    signal dismissRequested()

    objectName: "gatherOverviewSurface"
    focus: true

    Keys.onEscapePressed: function (event) {
        root.dismissRequested()
        event.accepted = true
    }

    // The scrim. Clicking it is how the overview is dismissed by pointer, so
    // it covers the whole surface and sits under every tile.
    Rectangle {
        objectName: "gatherScrim"
        anchors.fill: parent
        visible: root.available
        color: Tokens.ready ? Tokens.bg.base : "transparent"
        opacity: !Tokens.ready ? 0
                 : Tokens.accessibility.reducedTransparency
                   || Tokens.accessibility.highContrast
                   ? 1
                   : Tokens.material.popup.opacity

        TapHandler {
            objectName: "gatherScrimDismiss"
            onTapped: root.dismissRequested()
        }

        WheelHandler {
            objectName: "gatherScrollHandler"
            // The grid is the only thing that scrolls, so the wheel is inert
            // when the field left no room for a single column.
            enabled: root.gridColumns > 0
            onWheel: function (event) {
                if (event.angleDelta.y === 0)
                    return
                // A wheel notch is 120 eighths of a degree by convention; one
                // notch moves one grid row, which is the row pitch the planner
                // produced. Derived in one place so the wheel and any
                // "more below" hint cannot disagree about it.
                const rows = -event.angleDelta.y / 120
                root.scrollRequested(rows * root.gridRowExtent)
            }
        }
    }

    // Every visible item, drawn at the frame the planner chose. The projection
    // already dropped invisible placements, so there is nothing to filter.
    Repeater {
        objectName: "gatherTiles"
        model: root.available ? root.projection.items : []

        delegate: Item {
            id: tileHost
            required property var modelData

            objectName: "gatherTile_" + modelData.taskId
            x: modelData.frame.x - root.origin.x
            y: modelData.frame.y - root.origin.y
            width: modelData.frame.width
            height: modelData.frame.height

            Component {
                id: iconChipComponent
                GatherIconChip {
                    item: tileHost.modelData
                    interactive: root.interactive
                    reducedMotion: root.reducedMotion
                    onActivated: root.activated(tileHost.modelData)
                }
            }

            Component {
                id: containerCardComponent
                GatherContainerCard {
                    item: tileHost.modelData
                    interactive: root.interactive
                    reducedMotion: root.reducedMotion
                    onActivated: root.activated(tileHost.modelData)
                }
            }

            Component {
                id: windowTileComponent
                GatherWindowTile {
                    item: tileHost.modelData
                    interactive: root.interactive
                    reducedMotion: root.reducedMotion
                    onActivated: root.activated(tileHost.modelData)
                }
            }

            Loader {
                objectName: "gatherTileLoader"
                anchors.fill: parent
                sourceComponent: tileHost.modelData.lane === "icon"
                                 ? iconChipComponent
                                 : tileHost.modelData.lane === "card"
                                   ? containerCardComponent
                                   : windowTileComponent
            }
        }
    }

    // Nothing in the session. The field is still real, so say so inside it
    // rather than leaving a bare scrim.
    Text {
        objectName: "gatherEmptyState"
        visible: root.showsEmptyState
        x: root.fieldRect.x - root.origin.x
        y: root.fieldRect.y - root.origin.y
        width: root.fieldRect.width
        height: root.fieldRect.height
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: qsTr("No open windows")
        color: Tokens.ready ? Tokens.fg.muted : "transparent"
        font.family: Tokens.ready ? Tokens.type.fontFamily : ""
        font.pointSize: Tokens.ready ? Tokens.type.body : 1
    }

    // Truth the user would otherwise have to guess at: tiles below the fold,
    // and entries the task list's own presentation cap never handed over.
    Text {
        objectName: "gatherHiddenHint"
        visible: root.available && !root.showsEmptyState
                 && (root.hiddenWindows > 0 || root.sourceOverflow > 0)
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Tokens.ready ? Tokens.space["2"] : 0
        horizontalAlignment: Text.AlignRight
        text: root.sourceOverflow > 0
              ? qsTr("%1 more below · %2 not reported by the task list")
                .arg(root.hiddenWindows).arg(root.sourceOverflow)
              : qsTr("%1 more below").arg(root.hiddenWindows)
        color: Tokens.ready ? Tokens.fg.muted : "transparent"
        font.family: Tokens.ready ? Tokens.type.fontFamily : ""
        font.pointSize: Tokens.ready ? Tokens.type.caption : 1
    }

    // The source is fenced: the picture is the retained generation and no tile
    // will act. Saying so is the difference between "stale" and "broken".
    C.DegradedNotice {
        objectName: "gatherDegradedNotice"
        visible: root.available && !root.interactive
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: Tokens.ready ? Tokens.space["2"] : 0
        reason: qsTr("Window list unavailable — showing the last known windows")
    }
}
