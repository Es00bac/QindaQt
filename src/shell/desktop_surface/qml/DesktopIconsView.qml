// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// Desktop-directory icons: activation, marquee/keyboard multi-selection, drag
// placement, and selection-scoped file operations (Trash, clipboard cut/copy/
// paste) through the DesktopContentsController boundary. Every operation
// consumes the selected rows through that controller, which re-validates each
// entry against the listing-time identity before mutating anything.
//
// This view owns composition, input and the live drag. Two collaborators hold
// the rest: DesktopIconPlacement answers where each icon lives and which
// output draws it, and DesktopIconSelection owns the selected-id set.
//
// AGENT-CONTRACT (ADR-0167): the desktop is ONE desktop. Placements live in
// global layout coordinates in the shared DesktopIconLayoutStore, and every
// per-output surface renders only the icons its own output owns, so an icon
// appears exactly once across all outputs. Never reintroduce a per-output
// placement namespace: that is what produced two independent sets of icons on
// a two-output session.
Item {
    id: root
    required property var settings
    required property var contents
    required property var layoutStore
    required property string screenName

    // Global-frame {name, x, y, width, height, workArea} of every output and
    // the output unplaced icons flow onto, injected by DesktopSurfaceController
    // from screen truth and the panels' reservations (ADR-0261). Defaults keep
    // a single-surface host (tests, a one-output session) behaving exactly as a
    // local-coordinate surface.
    property var outputRects: []
    property string primaryOutputName: screenName
    // Programmatic movement glides; a dragged icon never does (see the tile).
    property bool animatePlacement: true

    readonly property bool placementRight: String(settings?.placement ?? "left") === "right"
    readonly property int iconSize: {
        const requested = Number(settings?.iconSize ?? 48)
        return Number.isNaN(requested) ? 48
             : Math.round(Math.min(128, Math.max(16, requested)))
    }
    readonly property real tileWidth: Math.max(iconSize + 24, 104)
    readonly property real tileHeight: iconSize + 40
    readonly property var rows: contents.rows
    readonly property bool canPaste: contents.canPaste === true
    readonly property bool snapToGrid: settings?.snapToGrid !== false

    readonly property DesktopIconPlacement placement: DesktopIconPlacement {
        rows: root.rows
        layoutStore: root.layoutStore
        screenName: root.screenName
        outputRects: root.outputRects
        primaryOutputName: root.primaryOutputName
        tileWidth: root.tileWidth
        tileHeight: root.tileHeight
        placementRight: root.placementRight
        surfaceWidth: root.width
        surfaceHeight: root.height
        animate: root.animatePlacement
    }
    readonly property DesktopIconSelection selection: DesktopIconSelection {
        rows: root.rows
    }

    // The surface contract and its tests read these off the view itself.
    readonly property var selectedIds: selection.selectedIds
    readonly property string selectedId: selection.currentId

    property string contextEntryId: ""
    property string contextEntryLabel: ""
    property bool contextEntryIsDirectory: false
    // The dock facade (ADR-0265) for "Add to Dock"; null without a dock.
    property var dockAccess: null

    function isSelected(entryId) { return selection.isSelected(entryId) }
    function selectionCount() { return selection.count() }
    function clearSelection() { selection.clear() }
    function selectAll() { selection.selectAll() }

    // --- file operations, all through the contents controller boundary -----
    function openEntry(entryId) { contents.open(entryId) }
    function trashSelection() {
        const picked = selection.selectedRows()
        if (picked.length > 0)
            contents.trashEntries(picked)
    }
    function cutSelectionOps() {
        const picked = selection.selectedRows()
        if (picked.length > 0)
            contents.cutSelection(picked)
    }
    function copySelectionOps() {
        const picked = selection.selectedRows()
        if (picked.length > 0)
            contents.copySelection(picked)
    }
    function pasteClipboard() { contents.pasteIntoDesktop() }
    // ADR-0273: File Manager's own dialogs (Get Info, Open With, New File) for
    // one icon, or for the Desktop folder when `entryId` is empty.
    function runFileManagerAction(actionId, entryId) {
        contents.runFileManagerAction(actionId, entryId)
    }
    // The whole selection joins the dock as one edit (the dock refuses a
    // second write while the first is saving).
    function addSelectionToDock() {
        if (dockAccess === null)
            return
        const paths = selection.selectedRows().map(row => String(row.path))
        if (paths.length > 0)
            dockAccess.addPaths(paths)
    }
    function reflow() {
        selection.clear()
        layoutStore.clearAll()
        contents.refresh()
    }

    // --- drag, owned by the surface holding the pointer grab ---------------
    property var dragKeys: []
    property var dragStart: ({})
    readonly property bool dragInFlight: dragKeys.length > 0
    function isDragKey(layoutKey) { return dragKeys.indexOf(layoutKey) >= 0 }
    // This output draws an icon it owns, and also any icon THIS surface is
    // dragging, so a delegate is never destroyed out from under the pointer
    // grab as the icon crosses an output boundary.
    function drawsRow(layoutKey) {
        return (dragInFlight && isDragKey(layoutKey))
            || placement.ownsRow(layoutKey)
    }

    function beginDrag(tile) {
        const keys = []
        const start = ({})
        const wholeSelection = selection.isSelected(tile.entryId)
        for (const row of rows) {
            const key = String(row.layoutKey)
            const included = wholeSelection ? selection.isSelected(row.id)
                                            : row.id === tile.entryId
            if (!included)
                continue
            const point = placement.positions[key]
            if (point === undefined)
                continue
            keys.push(key)
            start[key] = { x: point.x, y: point.y }
        }
        dragStart = start
        dragKeys = keys
    }
    function moveDrag(dx, dy) {
        // AGENT-GUARD: clamp the group's TRANSLATION once, not each icon
        // separately; per-icon clamping collapsed a multi-icon drag into a
        // pile at an edge. The bound is the work area (ADR-0261).
        const bounds = placement.workBounds
        let minX = -Infinity, maxX = Infinity, minY = -Infinity, maxY = Infinity
        for (const key of dragKeys) {
            const start = dragStart[key]
            minX = Math.max(minX, bounds.x - start.x)
            maxX = Math.min(maxX, bounds.x + bounds.width - tileWidth - start.x)
            minY = Math.max(minY, bounds.y - start.y)
            maxY = Math.min(maxY, bounds.y + bounds.height - tileHeight - start.y)
        }
        const moveX = minX > maxX ? dx : Math.max(minX, Math.min(maxX, dx))
        const moveY = minY > maxY ? dy : Math.max(minY, Math.min(maxY, dy))
        const batch = ({})
        for (const key of dragKeys) {
            const start = dragStart[key]
            batch[key] = { x: start.x + moveX, y: start.y + moveY }
        }
        layoutStore.updateDrag(batch)
    }
    function commitDrag() {
        for (const key of dragKeys) {
            const live = layoutStore.dragPosition(key)
            if (live.x === undefined)
                continue
            const dropped = { x: Number(live.x), y: Number(live.y) }
            const target = snapToGrid ? placement.snapGlobal(dropped, key, dragKeys)
                                      : placement.clampToWorkArea(dropped)
            layoutStore.setPosition(key, target.x, target.y)
        }
        dragKeys = []
        dragStart = ({})
        layoutStore.endDrag()
    }
    function cancelDrag(tile, modifiers) {
        const hadGroup = dragKeys.length > 1
        dragKeys = []
        dragStart = ({})
        layoutStore.endDrag()
        // Pressing an already-selected tile keeps the set for a potential
        // group drag; a plain click without movement collapses to it.
        if (modifiers === Qt.NoModifier && hadGroup)
            selection.selectOnly(tile.entryId)
    }

    // --- input surfaces ----------------------------------------------------
    function layoutTiles() {
        const tiles = []
        for (let i = 0; i < tileRepeater.count; ++i) {
            const item = tileRepeater.itemAt(i)
            // Only icons this output actually shows can be marquee-selected on
            // it; a hidden delegate standing in for another output's icon must
            // not be swept by a band drawn here.
            if (item !== null && item.visible)
                tiles.push({ id: item.entryId, item: item })
        }
        return tiles
    }
    function positionAnchor(anchor, pointerX, pointerY, popupWidth, popupHeight) {
        const x = Math.max(0, Math.min(pointerX, Math.max(0, width - popupWidth)))
        const y = Math.max(0, Math.min(pointerY, Math.max(0, height - popupHeight)))
        anchor.x = Math.max(0, x - anchor.width)
        anchor.y = y
    }
    function openIconMenu(tile, localX, localY) {
        contextEntryId = tile.entryId
        contextEntryLabel = tile.entryLabel
        contextEntryIsDirectory = tile.modelData.isDirectory === true
        const point = tile.mapToItem(root, localX, localY)
        positionAnchor(iconMenuAnchor, point.x, point.y,
                       iconContextMenu.width, iconContextMenu.height)
        iconContextMenu.popup()
    }

    Connections {
        target: root.contents
        function onRowsChanged() { root.selection.reconcile() }
    }

    // Desktop-wide keyboard gestures: Delete trashes the current selection
    // (recoverable, exactly like File Manager's Trash), Ctrl+A selects every
    // icon. Unhandled key presses from a focused tile propagate here.
    Keys.onDeletePressed: root.trashSelection()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) {
            root.selectAll()
            event.accepted = true
        }
    }

    // One-time upgrade of a superseded per-output arrangement. Only the
    // primary surface runs it, and it keeps that output's arrangement while
    // dropping the other outputs' conflicting copies - see ADR-0167 for why a
    // merge is not possible.
    Component.onCompleted: {
        if (root.screenName === root.primaryOutputName
                && root.layoutStore.hasLegacyLayout()) {
            root.layoutStore.migrateLegacyLayout(root.screenName,
                                                 root.placement.ownRect.x,
                                                 root.placement.ownRect.y)
        }
    }

    // Marquee input sits UNDER the tile layer: icon presses are consumed by
    // the tiles above it; only empty-desktop presses start a band here.
    DesktopMarqueeBand {
        id: marquee
        objectName: "desktopMarqueeBand"
        anchors.fill: parent
        tiles: root.layoutTiles
        onFinished: (ids, modifiers) => root.selection.applyMarquee(ids, modifiers)
    }

    Item {
        id: flow
        objectName: "desktopIconsFlow"
        anchors.fill: parent
        Repeater {
            id: tileRepeater
            model: root.rows
            DesktopIconTile {
                view: root
            }
        }
    }

    // The band rectangle paints ABOVE the tiles while a marquee is in flight.
    Rectangle {
        objectName: "desktopMarqueeBandVisual"
        x: marquee.bandX
        y: marquee.bandY
        width: marquee.bandWidth
        height: marquee.bandHeight
        visible: marquee.dragging && (marquee.bandWidth > 0 || marquee.bandHeight > 0)
        color: "#333b74dd"
        border.color: "#883b74dd"
        border.width: 1
    }

    Item {
        id: iconMenuAnchor
        objectName: "desktopIconContextMenuAnchor"
        width: 1
        height: 1
        DesktopIconContextMenu {
            id: iconContextMenu
            objectName: "desktopIconContextMenu"
            directory: root.contextEntryIsDirectory
            onOpenRequested: root.openEntry(root.contextEntryId)
            onOpenWithRequested: root.runFileManagerAction("file.open-with", root.contextEntryId)
            onInfoRequested: root.runFileManagerAction("file.properties", root.contextEntryId)
            onRenameRequested: renamePopup.begin(root.contextEntryId,
                                                  root.contextEntryLabel)
            onCutRequested: root.cutSelectionOps()
            onCopyRequested: root.copySelectionOps()
            onDeleteRequested: root.trashSelection()
            dockAvailable: root.dockAccess !== null
            desktopEntry: root.contextEntryId.endsWith(".desktop")
            onAddToDockRequested: root.addSelectionToDock()
        }
    }
    Item {
        id: renameAnchor
        anchors.centerIn: parent
        width: 1
        height: 1
        DesktopRenamePopup {
            id: renamePopup
            objectName: "desktopRenamePopup"
            onRenameRequested: (entryId, newName) => root.contents.rename(entryId, newName)
        }
    }
}
