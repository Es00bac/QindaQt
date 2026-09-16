// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// Where each desktop icon lives, and which output draws it (ADR-0167).
//
// AGENT-CONTRACT: placements are GLOBAL layout coordinates - the frame the
// compositor uses to lay outputs side by side - resolved from one shared
// DesktopIconLayoutStore. This object is pure resolution: it reads the store
// and the output geometry and answers questions. It never writes placements;
// the view's drag does that. Keeping the two apart is what stops a per-output
// placement namespace from creeping back in.
QtObject {
    id: placement

    required property var rows
    required property var layoutStore
    required property string screenName
    // {name, x, y, width, height} per connected output, in the global frame.
    required property var outputRects
    required property string primaryOutputName
    required property real tileWidth
    required property real tileHeight
    required property bool placementRight
    // This surface's own size, used only as the single-output fallback frame.
    required property real surfaceWidth
    required property real surfaceHeight
    // Policy switch from the view; animations also wait for a settled layout.
    required property bool animate

    readonly property real cellMargin: 6
    readonly property real cellSpacing: 4

    // This output's rectangle in the global frame. Without injected output
    // truth the surface is its own frame at the origin, which is exactly the
    // single-output behavior.
    readonly property var ownRect: {
        for (const candidate of outputRects) {
            if (String(candidate.name) === screenName)
                return candidate
        }
        return { name: screenName, x: 0, y: 0,
                 width: surfaceWidth, height: surfaceHeight }
    }
    readonly property var primaryRect: {
        for (const candidate of outputRects) {
            if (String(candidate.name) === primaryOutputName)
                return candidate
        }
        return ownRect
    }
    // Union of every output: the bound a drag may travel within.
    readonly property rect desktopBounds: {
        if (outputRects.length === 0)
            return Qt.rect(0, 0, surfaceWidth, surfaceHeight)
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        for (const candidate of outputRects) {
            left = Math.min(left, candidate.x)
            top = Math.min(top, candidate.y)
            right = Math.max(right, candidate.x + candidate.width)
            bottom = Math.max(bottom, candidate.y + candidate.height)
        }
        return Qt.rect(left, top, right - left, bottom - top)
    }

    // Bumped whenever the shared store changes, because store lookups are
    // function calls and cannot be property dependencies on their own.
    property int revision: 0
    readonly property Connections storeConnections: Connections {
        target: placement.layoutStore
        function onChanged() { placement.revision++ }
        function onDragChanged() { placement.revision++ }
    }

    // AGENT-GUARD: placement animations stay OFF until the surface has a real
    // size and one layout pass has run, and switch off again across any
    // resize. Without this every icon visibly flies in from the window origin
    // on shell startup and again after any output reconfiguration, because the
    // default slots are derived from the surface geometry and that geometry
    // arrives after the delegates exist.
    property bool animationsLive: false
    readonly property Timer settleTimer: Timer {
        interval: 0
        onTriggered: placement.animationsLive = placement.animate
    }
    function resettle() {
        animationsLive = false
        if (surfaceWidth > 0 && surfaceHeight > 0)
            settleTimer.restart()
    }
    onSurfaceWidthChanged: resettle()
    onSurfaceHeightChanged: resettle()
    Component.onCompleted: resettle()

    // Resolved global position per layout key: a live drag wins, then the
    // saved placement, then the default slot on the primary output.
    readonly property var positions: {
        const currentRevision = placement.revision
        void currentRevision
        const resolved = ({})
        // Bindings can evaluate once before the injected store is assigned.
        if (!placement.layoutStore)
            return resolved
        for (let index = 0; index < placement.rows.length; ++index) {
            const key = String(placement.rows[index].layoutKey)
            const dragged = placement.layoutStore.dragPosition(key)
            if (dragged.x !== undefined) {
                resolved[key] = { x: Number(dragged.x), y: Number(dragged.y) }
                continue
            }
            const stored = placement.layoutStore.position(key)
            if (stored.x !== undefined) {
                resolved[key] = { x: Number(stored.x), y: Number(stored.y) }
                continue
            }
            resolved[key] = placement.defaultGlobalPosition(index)
        }
        return resolved
    }

    // Default slot for the icon at listing index `slot`. Always on the primary
    // output: an unplaced icon has to land somewhere predictable, and "the
    // user's main screen" is that place.
    //
    // AGENT-GUARD: keyed on the icon's index in the listing, NOT on a running
    // count of unplaced icons. Counting only unplaced ones makes every
    // remaining icon jump one slot the moment the user places any single icon.
    function defaultGlobalPosition(slot) {
        const area = primaryRect
        const rowsPerColumn = Math.max(1, Math.floor(
            (area.height - 2 * cellMargin + cellSpacing) / (tileHeight + cellSpacing)))
        const column = Math.floor(slot / rowsPerColumn)
        const line = slot % rowsPerColumn
        const localOffset = placementRight
            ? area.width - cellMargin - tileWidth - column * (tileWidth + cellSpacing)
            : cellMargin + column * (tileWidth + cellSpacing)
        return {
            x: area.x + localOffset,
            y: area.y + cellMargin + line * (tileHeight + cellSpacing)
        }
    }

    // The output that owns a global placement. The tile CENTRE decides, so an
    // icon straddling a seam belongs to the output showing most of it; a point
    // outside every output (an output was unplugged) falls to the primary, so
    // an icon can never become unreachable.
    function ownerOf(point) {
        const centreX = point.x + tileWidth / 2
        const centreY = point.y + tileHeight / 2
        for (const candidate of outputRects) {
            if (centreX >= candidate.x && centreX < candidate.x + candidate.width
                    && centreY >= candidate.y && centreY < candidate.y + candidate.height)
                return String(candidate.name)
        }
        return primaryOutputName
    }
    function rectFor(outputName) {
        for (const candidate of outputRects) {
            if (String(candidate.name) === outputName)
                return candidate
        }
        return primaryRect
    }
    // Whether THIS output draws a given icon, ignoring any drag in flight.
    function ownsRow(layoutKey) {
        const point = positions[layoutKey]
        if (point === undefined)
            return false
        return ownerOf(point) === screenName
    }
    function localX(layoutKey) {
        const point = positions[layoutKey]
        return point === undefined ? 0 : point.x - ownRect.x
    }
    function localY(layoutKey) {
        const point = positions[layoutKey]
        return point === undefined ? 0 : point.y - ownRect.y
    }

    // Nearest FREE grid cell on the output that will own the drop, so a drop
    // onto an occupied cell pushes outward instead of stacking icons.
    // `movingKeys` are excluded from occupancy: they are the icons being
    // placed by this same gesture.
    function snapGlobal(point, layoutKey, movingKeys) {
        const area = rectFor(ownerOf(point))
        const cellWidth = tileWidth + cellSpacing
        const cellHeight = tileHeight + cellSpacing
        const columns = Math.max(1, Math.floor(
            (area.width - 2 * cellMargin + cellSpacing) / cellWidth))
        const lines = Math.max(1, Math.floor(
            (area.height - 2 * cellMargin + cellSpacing) / cellHeight))
        const wantColumn = Math.round((point.x - area.x - cellMargin) / cellWidth)
        const wantLine = Math.round((point.y - area.y - cellMargin) / cellHeight)

        const taken = ({})
        for (const row of rows) {
            const key = String(row.layoutKey)
            if (key === layoutKey || movingKeys.indexOf(key) >= 0)
                continue
            const other = positions[key]
            if (other === undefined || ownerOf(other) !== String(area.name))
                continue
            taken[Math.round((other.x - area.x - cellMargin) / cellWidth) + ":"
                  + Math.round((other.y - area.y - cellMargin) / cellHeight)] = true
        }

        let best = null
        let bestDistance = Infinity
        for (let column = 0; column < columns; ++column) {
            for (let line = 0; line < lines; ++line) {
                if (taken[column + ":" + line] === true)
                    continue
                const distance = (column - wantColumn) * (column - wantColumn)
                              + (line - wantLine) * (line - wantLine)
                if (distance < bestDistance) {
                    bestDistance = distance
                    best = { column: column, line: line }
                }
            }
        }
        if (best === null)
            return point
        return {
            x: area.x + cellMargin + best.column * cellWidth,
            y: area.y + cellMargin + best.line * cellHeight
        }
    }
}
