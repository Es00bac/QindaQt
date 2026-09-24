// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import "DockDropGeometry.js" as DockDrop

// Gesture state of the dock strip (ADR-0265): which tile is being dragged,
// where a drop would land, and which dock-facade call each finished gesture
// makes. Positions are visual (row) positions; every call converts them to
// dock-value indices through the rows' own `index`.
//
// AGENT-GUARD: a gesture only ever chooses one facade call. Nothing here
// edits rows or keeps a second copy of the dock; the facade's next rows are
// the only truth, so a refused edit simply leaves the dock as it was.
QtObject {
    id: gestures

    required property var access
    property bool ready: false
    property var rows: []
    property bool vertical: false
    property real slotExtent: 1
    property real tileExtent: 1
    // The strip's extent across the panel; a tile dragged a whole tile
    // beyond it on either side is removed on release.
    property real crossExtent: 0

    property int dragFrom: -1
    property real dragTravel: 0
    property int hoverGap: -1
    property int hoverTarget: -1
    property bool dragRemoving: false

    // Keyboard moves ask the strip to keep focus on the moved tile.
    signal focusRequested(int position)

    function mainAxis(point) {
        return vertical ? point.y : point.x
    }

    // Translated from the stable identity the launcher publishes; keep in
    // sync with LauncherSection.qml while the shapes match.
    function categoryTitle(identity) {
        switch (identity) {
        case "utilities": return qsTr("Utilities")
        case "development": return qsTr("Development")
        case "education": return qsTr("Education")
        case "games": return qsTr("Games")
        case "graphics": return qsTr("Graphics")
        case "audioVideo": return qsTr("Audio & Video")
        case "network": return qsTr("Network")
        case "office": return qsTr("Office")
        case "science": return qsTr("Science")
        case "settings": return qsTr("Settings")
        case "system": return qsTr("System")
        default: return ""
        }
    }

    // Two applications of one launcher category name their group after it
    // ("Office"); anything else starts as "Group" and can be renamed.
    function groupNameFor(target, otherCategory) {
        const category = String(target.categoryIdentity ?? "")
        const title = categoryTitle(category)
        return title.length > 0 && category === String(otherCategory ?? "")
            ? title : qsTr("Group")
    }

    // Keyboard parity for dragging: the item trades places with its visible
    // neighbour.
    function moveRow(position, direction) {
        const target = position + direction
        if (!ready || position < 0 || target < 0 || target >= rows.length)
            return
        const gap = direction < 0 ? target : target + 1
        if (access.moveItem(Number(rows[position].index),
                            DockDrop.storageGap(rows, gap, access.itemCount)))
            focusRequested(target)
    }

    function dragReset() {
        dragFrom = -1
        dragTravel = 0
        hoverGap = -1
        hoverTarget = -1
        dragRemoving = false
    }

    function dragBegin(pressPoint) {
        dragReset()
        dragFrom = DockDrop.tileAt(mainAxis(pressPoint), slotExtent, rows.length)
        hoverGap = dragFrom
    }

    function dragUpdate(point, pressPoint) {
        if (dragFrom < 0)
            return
        dragTravel = mainAxis(point) - mainAxis(pressPoint)
        const cross = vertical ? point.x : point.y
        dragRemoving = cross < -tileExtent || cross > crossExtent + tileExtent
        const position = mainAxis(point)
        const over = DockDrop.slotAt(position, slotExtent, tileExtent, rows.length, true)
        if (over.target >= 0 && over.target !== dragFrom
                && DockDrop.canMerge(rows[dragFrom], rows[over.target])) {
            hoverTarget = over.target
            hoverGap = -1
            return
        }
        hoverTarget = -1
        hoverGap = DockDrop.slotAt(position, slotExtent, tileExtent, rows.length, false).gap
    }

    function dragEnd() {
        const from = dragFrom
        const gap = hoverGap
        const target = hoverTarget
        const removing = dragRemoving
        dragReset()
        if (from < 0 || from >= rows.length || !ready)
            return
        const source = rows[from]
        if (removing)
            access.removeItem(Number(source.index))
        else if (target >= 0)
            access.combineItems(Number(rows[target].index), Number(source.index),
                                groupNameFor(rows[target], source.categoryIdentity))
        else if (gap >= 0 && gap !== from && gap !== from + 1)
            access.moveItem(Number(source.index),
                            DockDrop.storageGap(rows, gap, access.itemCount))
    }

    function dropKind(drag) {
        const formats = drag.formats ?? []
        if (formats.indexOf(DockDrop.memberFormat) >= 0)
            return "member"
        if (formats.indexOf(DockDrop.applicationFormat) >= 0)
            return "application"
        return drag.hasUrls ? "urls" : ""
    }

    function dropHover(point, kind) {
        const position = mainAxis(point)
        if (kind === "application") {
            const over = DockDrop.slotAt(position, slotExtent, tileExtent, rows.length, true)
            if (over.target >= 0
                    && DockDrop.canMerge({ "kind": "application" }, rows[over.target])) {
                hoverTarget = over.target
                hoverGap = -1
                return
            }
        }
        hoverTarget = -1
        hoverGap = DockDrop.slotAt(position, slotExtent, tileExtent, rows.length, false).gap
    }

    // Returns whether the facade took the drop; the caller accepts or
    // refuses the platform drop accordingly.
    function dropCommit(drop) {
        const kind = dropKind(drop)
        const target = hoverTarget
        const at = DockDrop.storageGap(rows, hoverGap < 0 ? rows.length : hoverGap,
                                       ready ? access.itemCount : 0)
        dragReset()
        if (!ready)
            return false
        if (kind === "application") {
            const entryId = String(drop.getDataAsString(DockDrop.applicationFormat)).trim()
            return target >= 0
                ? access.combineWithApplication(Number(rows[target].index), entryId,
                      groupNameFor(rows[target], access.applicationCategory(entryId)))
                : access.insertApplication(at, entryId)
        }
        if (kind === "member") {
            let member = null
            try {
                member = JSON.parse(drop.getDataAsString(DockDrop.memberFormat))
            } catch (error) {
                member = null
            }
            return member !== null
                && access.moveOutOfGroup(Number(member.group), String(member.entryId), at)
        }
        return kind === "urls" && access.insertUrls(at, drop.urls)
    }

    // Transform-only shift of the tile at `position` (see DockItemTile).
    function shiftFor(position) {
        if (dragFrom >= 0 && position === dragFrom)
            return dragTravel
        if (hoverTarget >= 0)
            return 0
        return DockDrop.shiftFor(position, dragFrom, hoverGap, slotExtent)
    }
}
