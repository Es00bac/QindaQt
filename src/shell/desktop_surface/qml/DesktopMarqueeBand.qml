// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Rubber-band (marquee) selection input for the desktop icons view. Declared
// BELOW the tile layer inside DesktopIconsView, so presses that land on an
// icon are consumed by that icon's own MouseArea and only empty-desktop
// presses start a band here. On release the intersecting icon ids are
// reported through `finished`; DesktopIconsView applies them with the same
// modifier policy as every other selection gesture (no modifier replaces,
// Shift unions, Control toggles). A press-release without a drag reports an
// empty id set, which the view treats as an empty-space click (clear the
// selection). The band rectangle itself is painted by the owning view above
// the tiles; this component owns input and geometry only.
Item {
    id: root

    // Function returning the selectable tiles as [{id, item}]; the items
    // live in the same coordinate space as this band.
    required property var tiles
    signal finished(var ids, int modifiers)

    readonly property bool dragging: bandArea.pressed
    property real originX: 0
    property real originY: 0
    property real bandX: 0
    property real bandY: 0
    property real bandWidth: 0
    property real bandHeight: 0

    function updateBand(pointerX, pointerY) {
        const clampedX = Math.max(0, Math.min(pointerX, root.width))
        const clampedY = Math.max(0, Math.min(pointerY, root.height))
        root.bandX = Math.min(root.originX, clampedX)
        root.bandY = Math.min(root.originY, clampedY)
        root.bandWidth = Math.abs(clampedX - root.originX)
        root.bandHeight = Math.abs(clampedY - root.originY)
    }

    function resetBand() {
        root.bandWidth = 0
        root.bandHeight = 0
    }

    function intersectingIds() {
        const ids = []
        const list = root.tiles()
        for (const candidate of list) {
            const item = candidate.item
            const overlapX = Math.min(root.bandX + root.bandWidth,
                                      item.x + item.width) - Math.max(root.bandX, item.x)
            const overlapY = Math.min(root.bandY + root.bandHeight,
                                      item.y + item.height) - Math.max(root.bandY, item.y)
            // A few pixels of overlap are accidental (shadow/aa); count only
            // meaningful crossings so an edge graze does not select.
            if (overlapX > 2 && overlapY > 2)
                ids.push(candidate.id)
        }
        return ids
    }

    MouseArea {
        id: bandArea
        anchors.fill: parent
        // Right/middle clicks fall through to the surface input below
        // exactly as before; only the left button starts a band.
        acceptedButtons: Qt.LeftButton

        onPressed: (mouse) => {
            root.originX = mouse.x
            root.originY = mouse.y
            root.updateBand(mouse.x, mouse.y)
        }
        onPositionChanged: (mouse) => {
            if (pressed)
                root.updateBand(mouse.x, mouse.y)
        }
        onReleased: (mouse) => {
            const dragged = root.bandWidth > 3 || root.bandHeight > 3
            const ids = dragged ? root.intersectingIds() : []
            root.resetBand()
            root.finished(ids, mouse.modifiers)
        }
        onCanceled: root.resetBand()
    }
}
