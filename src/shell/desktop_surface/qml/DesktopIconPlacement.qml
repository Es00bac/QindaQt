// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// Where each desktop icon lives, and which output draws it (ADR-0167), kept
// inside the part of each output the shell's panels leave free (ADR-0261).
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
    // {name, x, y, width, height, workArea} per connected output, in the
    // global frame. `workArea` ({x, y, width, height}) is the output minus the
    // panels' exclusive zones; an entry without one is all work area.
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
    readonly property var primaryWorkArea: workAreaOf(primaryRect)
    // Bounding box of every output's work area: the bound a drag's group
    // translation is clamped to. On one output that IS its work area, so a
    // drag stops at a panel's edge; across outputs the box may still span a
    // panel on a seam, and the drop is clamped by its owning output instead.
    readonly property rect workBounds: {
        if (outputRects.length === 0)
            return Qt.rect(0, 0, surfaceWidth, surfaceHeight)
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        for (const candidate of outputRects) {
            const area = workAreaOf(candidate)
            left = Math.min(left, area.x)
            top = Math.min(top, area.y)
            right = Math.max(right, area.x + area.width)
            bottom = Math.max(bottom, area.y + area.height)
        }
        return Qt.rect(left, top, right - left, bottom - top)
    }

    // AGENT-CONTRACT (ADR-0261): `workArea` is published by
    // DesktopSurfaceController from the exclusive zones the shell runtime's
    // panel plan actually requested. A missing or empty one (tests, the
    // single-surface fallback, a shell whose first plan is not in yet) means
    // the whole output, which is exactly the behavior before work areas.
    function workAreaOf(output) {
        const area = output.workArea
        if (area === undefined || area === null
                || !(Number(area.width) > 0) || !(Number(area.height) > 0))
            return output
        return area
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
    // saved placement clamped into its output's work area, then the default
    // slot on the primary output.
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
                resolved[key] = placement.clampToWorkArea(
                    { x: Number(stored.x), y: Number(stored.y) })
                continue
            }
            resolved[key] = placement.defaultGlobalPosition(index)
        }
        return resolved
    }

    // Default slot for the icon at listing index `slot`. Always on the primary
    // output, flowed inside its work area: an unplaced icon has to land
    // somewhere predictable, and "the user's main screen" is that place.
    //
    // AGENT-GUARD: keyed on the icon's index in the listing, NOT on a running
    // count of unplaced icons. Counting only unplaced ones makes every
    // remaining icon jump one slot the moment the user places any single icon.
    function defaultGlobalPosition(slot) {
        const area = primaryWorkArea
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

    // A tile wholly inside its owning output's work area keeps its position;
    // one crossing a work-area edge (saved under a panel, or dropped beside a
    // panel on a seam) lands cellMargin inside that edge, level with the
    // default grid - so an icon saved under the top bar shows just below it.
    //
    // AGENT-GUARD (ADR-0261): this runs at RESOLUTION and never writes the
    // store. A panel that later shrinks, moves or auto-hides gives the icon
    // its saved place back, and no other icon's saved position is rewritten to
    // make room. Do not apply it to live drag positions either: the view
    // clamps a drag's group translation once, because clamping each icon
    // separately collapses a group drag into a pile at an edge.
    function clampToWorkArea(point) {
        const area = workAreaOf(rectFor(ownerOf(point)))
        return {
            x: clampAxis(point.x, area.x, area.x + area.width, tileWidth),
            y: clampAxis(point.y, area.y, area.y + area.height, tileHeight)
        }
    }
    function clampAxis(value, low, high, size) {
        if (high - low < size)
            return low
        if (value < low)
            return Math.min(low + cellMargin, high - size)
        if (value + size > high)
            return Math.max(high - cellMargin - size, low)
        return value
    }

    // Nearest FREE grid cell in the work area of the output that will own the
    // drop, so a drop onto an occupied cell pushes outward instead of stacking
    // icons, and a drop onto a panel lands beside it. `movingKeys` are
    // excluded from occupancy: they are the icons placed by this same gesture.
    function snapGlobal(point, layoutKey, movingKeys) {
        const output = rectFor(ownerOf(point))
        const area = workAreaOf(output)
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
            if (other === undefined || ownerOf(other) !== String(output.name))
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
            return clampToWorkArea(point)
        return {
            x: area.x + cellMargin + best.column * cellWidth,
            y: area.y + cellMargin + best.line * cellHeight
        }
    }
}
