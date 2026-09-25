// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Rubber-band (marquee) selection shared by the four views. The
// owning view layers this above the entry view: presses that land on a
// delegate are declined (mouse.accepted = false) so the delegate's ordinary
// click/drag handling below is unaffected, while presses on empty viewport
// space draw a band. On release the band reports the intersecting delegate
// indexes through `finished`; the owner applies them through the same
// EntrySelection policy every other gesture uses (no modifier replaces, Shift
// unions, Control toggles). A press-release without a drag reports an empty
// index set, which the owner treats as an empty-space click.
//
// AGENT-NOTE: an overlay MouseArea with hoverEnabled left false does not
// intercept hover delivery — pointer frames still reach the delegates below
// (verified on the offscreen platform), so marquee support does not regress
// the row/cell hover affordance.
Item {
    id: root
    objectName: "selectionBand"

    required property var view
    // ADR-0270: maps a delegate's own index to an entry index, or -1 for a
    // row that is not an entry (a Details group heading). Null keeps the
    // delegate index, as the Icons and Columns views' delegates are entries.
    property var mapIndex: null
    signal finished(var indexes, int modifiers)

    readonly property bool dragging: bandArea.pressed
    property real originX: 0
    property real originY: 0
    property real bandX: 0
    property real bandY: 0
    property real bandWidth: 0
    property real bandHeight: 0

    // Delegate roots of the flickable view; every view's delegate (and the
    // Details table's rows) declares `required property int index`, so the
    // model index is readable from here as an ordinary property.
    function delegatesUnder(x, y, width, height) {
        const hits = []
        const content = root.view.contentItem
        for (let i = 0; i < content.children.length; ++i) {
            const child = content.children[i]
            // Only view delegates carry the model index; pooled or helper
            // children must never leak into the selection.
            if (typeof child.index !== "number" || !child.visible
                    || child.width <= 0 || child.height <= 0)
                continue
            const pos = child.mapToItem(root, 0, 0)
            const overlapX = Math.min(x + width, pos.x + child.width) - Math.max(x, pos.x)
            const overlapY = Math.min(y + height, pos.y + child.height) - Math.max(y, pos.y)
            // A few pixels of overlap are accidental (antialiased edges); a
            // real marquee crossing counts only meaningful intersections.
            if (overlapX > 2 && overlapY > 2) {
                const index = root.mapIndex ? root.mapIndex(child.index) : child.index
                if (index >= 0)
                    hits.push(index)
            }
        }
        hits.sort((a, b) => a - b)
        return hits
    }

    function itemAtPoint(x, y) {
        const content = root.view.contentItem
        for (let i = 0; i < content.children.length; ++i) {
            const child = content.children[i]
            if (typeof child.index !== "number" || !child.visible
                    || child.width <= 0 || child.height <= 0)
                continue
            const pos = child.mapToItem(root, 0, 0)
            if (x >= pos.x && x <= pos.x + child.width
                    && y >= pos.y && y <= pos.y + child.height)
                return child
        }
        return null
    }

    function updateBand(pointerX, pointerY) {
        const clampedX = Math.max(0, Math.min(pointerX, root.width))
        const clampedY = Math.max(0, Math.min(pointerY, root.height))
        root.bandX = Math.min(root.originX, clampedX)
        root.bandY = Math.min(root.originY, clampedY)
        root.bandWidth = Math.abs(clampedX - root.originX)
        root.bandHeight = Math.abs(clampedY - root.originY)
    }

    MouseArea {
        id: bandArea
        anchors.fill: parent
        // Right/middle clicks and wheel events must reach the view and the
        // background handlers exactly as before; only the left button starts
        // a band.
        acceptedButtons: Qt.LeftButton

        onPressed: (mouse) => {
            if (root.itemAtPoint(mouse.x, mouse.y) !== null) {
                mouse.accepted = false
                return
            }
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
            const indexes = dragged
                ? root.delegatesUnder(root.bandX, root.bandY,
                                      root.bandWidth, root.bandHeight)
                : []
            root.bandWidth = 0
            root.bandHeight = 0
            root.finished(indexes, mouse.modifiers)
        }
        onCanceled: {
            root.bandWidth = 0
            root.bandHeight = 0
        }
    }

    Rectangle {
        id: bandVisual
        x: root.bandX
        y: root.bandY
        width: root.bandWidth
        height: root.bandHeight
        visible: root.bandWidth > 0 && root.bandHeight > 0 && root.dragging
        color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.20)
        border.color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.60)
        border.width: 1
    }
}
