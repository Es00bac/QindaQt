// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Desktop-directory icons with free placement, persistent per-output
// geometry, activation, marquee/keyboard multi-selection, and
// selection-scoped file operations (Trash, clipboard cut/copy/paste) through
// the DesktopContentsController boundary. Selection state lives here as the
// `selectedIds` set plus the current/anchor `selectedId`; every operation
// consumes the selected rows through the controller, which re-validates each
// entry against the listing-time identity before mutating anything.
Item {
    id: root
    required property var settings
    required property var contents
    required property var layoutStore
    required property string screenName

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
    // Multi-selection: the id set, the current entry, and the shift-range
    // anchor. `selectedId` remains the single "current" id the surface
    // contract and its tests read after a plain click.
    property var selectedIds: ({})
    property string selectedId: ""
    property string anchorId: ""
    property string contextEntryId: ""
    property string contextEntryLabel: ""

    function clearSelection() {
        selectedIds = ({})
        selectedId = ""
        anchorId = ""
    }
    function isSelected(entryId) { return selectedIds[entryId] !== undefined }
    function selectionCount() { return Object.keys(selectedIds).length }
    function selectedRows() {
        const rows = []
        for (const row of root.rows) {
            if (isSelected(row.id))
                rows.push(row)
        }
        return rows
    }
    function selectOnly(tile) {
        const next = ({})
        next[tile.entryId] = true
        selectedIds = next
        selectedId = tile.entryId
        anchorId = tile.entryId
    }
    function toggle(tile) {
        const next = Object.assign({}, selectedIds)
        if (next[tile.entryId] !== undefined) {
            delete next[tile.entryId]
            if (selectedId === tile.entryId)
                selectedId = ""
        } else {
            next[tile.entryId] = true
            selectedId = tile.entryId
            anchorId = tile.entryId
        }
        selectedIds = next
    }
    function rangeTo(tile) {
        const entries = root.rows
        let anchor = -1
        for (let i = 0; i < entries.length; ++i) {
            if (entries[i].id === root.anchorId) {
                anchor = i
                break
            }
        }
        let target = -1
        for (let i = 0; i < entries.length; ++i) {
            if (entries[i].id === tile.entryId) {
                target = i
                break
            }
        }
        if (target < 0)
            return
        if (anchor < 0)
            anchor = target
        const next = Object.assign({}, selectedIds)
        for (let i = Math.min(anchor, target); i <= Math.max(anchor, target); ++i)
            next[entries[i].id] = true
        selectedIds = next
        selectedId = tile.entryId
    }
    function selectAll() {
        const next = ({})
        for (const row of root.rows)
            next[row.id] = true
        selectedIds = next
        if (root.rows.length > 0) {
            selectedId = root.rows[root.rows.length - 1].id
            anchorId = selectedId
        }
    }
    function applyMarquee(ids, modifiers) {
        if (modifiers & Qt.ControlModifier) {
            const next = Object.assign({}, selectedIds)
            for (const id of ids) {
                if (next[id] !== undefined)
                    delete next[id]
                else
                    next[id] = true
            }
            selectedIds = next
            if (ids.length > 0)
                selectedId = ids[ids.length - 1]
        } else if (modifiers & Qt.ShiftModifier) {
            if (ids.length === 0)
                return
            const next = Object.assign({}, selectedIds)
            for (const id of ids)
                next[id] = true
            selectedIds = next
            selectedId = ids[ids.length - 1]
        } else {
            const next = ({})
            for (const id of ids)
                next[id] = true
            selectedIds = next
            selectedId = ids.length > 0 ? ids[ids.length - 1] : ""
            anchorId = selectedId
        }
    }
    function openEntry(entryId) { contents.open(entryId) }
    function trashSelection() {
        const rows = selectedRows()
        if (rows.length > 0)
            contents.trashEntries(rows)
    }
    function cutSelectionOps() {
        const rows = selectedRows()
        if (rows.length > 0)
            contents.cutSelection(rows)
    }
    function copySelectionOps() {
        const rows = selectedRows()
        if (rows.length > 0)
            contents.copySelection(rows)
    }
    function pasteClipboard() { contents.pasteIntoDesktop() }
    function fallbackPosition(index) {
        const margin = 6
        const spacing = 4
        const rowsPerColumn = Math.max(1, Math.floor((height - 2 * margin + spacing)
                                                     / (tileHeight + spacing)))
        const column = Math.floor(index / rowsPerColumn)
        const row = index % rowsPerColumn
        return {
            x: placementRight
               ? width - margin - tileWidth - column * (tileWidth + spacing)
               : margin + column * (tileWidth + spacing),
            y: margin + row * (tileHeight + spacing)
        }
    }
    function clampX(x) { return Math.max(0, Math.min(x, Math.max(0, width - tileWidth))) }
    function clampY(y) { return Math.max(0, Math.min(y, Math.max(0, height - tileHeight))) }
    function restoreTile(tile) {
        const stored = layoutStore.position(screenName, tile.layoutKey)
        const fallback = fallbackPosition(tile.index)
        tile.x = clampX(stored.x === undefined ? fallback.x : Number(stored.x))
        tile.y = clampY(stored.y === undefined ? fallback.y : Number(stored.y))
    }
    function restoreAllTiles() {
        for (let i = 0; i < tileRepeater.count; ++i) {
            const tile = tileRepeater.itemAt(i)
            if (tile !== null)
                restoreTile(tile)
        }
    }
    function reflow() {
        clearSelection()
        layoutStore.clearScreen(screenName)
        contents.refresh()
        Qt.callLater(restoreAllTiles)
    }
    function captureGroupStart() {
        const start = []
        for (let i = 0; i < tileRepeater.count; ++i) {
            const item = tileRepeater.itemAt(i)
            if (item !== null && isSelected(item.entryId))
                start.push({ tile: item, x: item.x, y: item.y })
        }
        return start
    }
    function layoutTiles() {
        const tiles = []
        for (let i = 0; i < tileRepeater.count; ++i) {
            const item = tileRepeater.itemAt(i)
            if (item !== null)
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
        const point = tile.mapToItem(root, localX, localY)
        positionAnchor(iconMenuAnchor, point.x, point.y,
                       iconContextMenu.width, iconContextMenu.height)
        iconContextMenu.popup()
    }

    // Any directory change (refresh, trash, paste) revalidates selection:
    // ids that no longer name a listed entry drop out, mirroring the File
    // Manager selection reconcile contract.
    Connections {
        target: root.contents
        function onRowsChanged() {
            const next = ({})
            for (const row of root.rows) {
                if (root.isSelected(row.id))
                    next[row.id] = true
            }
            if (Object.keys(next).length !== Object.keys(root.selectedIds).length)
                root.selectedIds = next
            if (root.selectedId !== "" && next[root.selectedId] === undefined)
                root.selectedId = ""
        }
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

    onWidthChanged: Qt.callLater(restoreAllTiles)
    onHeightChanged: Qt.callLater(restoreAllTiles)
    Component.onCompleted: Qt.callLater(restoreAllTiles)

    // Marquee input sits UNDER the tile layer: icon presses are consumed by
    // the tiles above it; only empty-desktop presses start a band here.
    DesktopMarqueeBand {
        id: marquee
        objectName: "desktopMarqueeBand"
        anchors.fill: parent
        tiles: root.layoutTiles
        onFinished: (ids, modifiers) => root.applyMarquee(ids, modifiers)
    }

    Item {
        id: flow
        objectName: "desktopIconsFlow"
        anchors.fill: parent
        Repeater {
            id: tileRepeater
            model: root.rows
            Rectangle {
                id: tile
                objectName: "desktopIconsTile"
                required property var modelData
                required property int index
                readonly property string entryId: String(modelData.id)
                readonly property string entryLabel: String(modelData.label)
                readonly property string layoutKey: String(modelData.layoutKey)
                readonly property bool selected: root.isSelected(entryId)
                property bool dragged: false
                width: root.tileWidth
                height: root.tileHeight
                radius: 4
                color: selected ? "#33ffffff"
                      : tileInput.containsMouse ? "#22ffffff" : "transparent"
                // The delegate can complete before its containing Window has
                // received output geometry. Defer once so every fallback is
                // computed against the real surface rather than stacking at
                // 0,0 during construction.
                Component.onCompleted: Qt.callLater(() => root.restoreTile(tile))
                Keys.onReturnPressed: root.openEntry(entryId)
                Keys.onEnterPressed: root.openEntry(entryId)

                MouseArea {
                    id: tileInput
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor

                    property real pressX: 0
                    property real pressY: 0
                    property bool moved: false
                    // Start positions of every selected tile while a group
                    // drag is in flight; empty for a no-selection drag.
                    property var groupStart: []

                    onPressed: (mouse) => {
                        tile.dragged = false
                        moved = false
                        if (mouse.button === Qt.LeftButton) {
                            if (mouse.modifiers & Qt.ControlModifier) {
                                root.toggle(tile)
                            } else if (mouse.modifiers & Qt.ShiftModifier) {
                                root.rangeTo(tile)
                            } else if (!root.isSelected(tile.entryId)) {
                                root.selectOnly(tile)
                            }
                            pressX = mouse.x
                            pressY = mouse.y
                            groupStart = root.isSelected(tile.entryId)
                                         ? root.captureGroupStart() : []
                            tile.forceActiveFocus(Qt.MouseFocusReason)
                        } else if (mouse.button === Qt.RightButton
                                   && !root.isSelected(tile.entryId)) {
                            root.selectOnly(tile)
                        }
                        // AGENT-GUARD: Qt.MiddleButton stays claimed as a
                        // no-op so a middle click over a tile never falls
                        // through to the surface input's Applications popup.
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed || groupStart.length === 0)
                            return
                        const dx = mouse.x - pressX
                        const dy = mouse.y - pressY
                        if (!moved && Math.abs(dx) + Math.abs(dy) < 3)
                            return
                        moved = true
                        tile.dragged = true
                        for (const entry of groupStart) {
                            entry.tile.x = root.clampX(entry.x + dx)
                            entry.tile.y = root.clampY(entry.y + dy)
                        }
                    }
                    onReleased: (mouse) => {
                        if (mouse.button !== Qt.LeftButton)
                            return
                        if (tile.dragged) {
                            // Persist the whole group; a rename keeps each
                            // tile's identity-keyed position.
                            for (const entry of groupStart) {
                                root.layoutStore.setPosition(root.screenName,
                                                             entry.tile.layoutKey,
                                                             entry.tile.x, entry.tile.y)
                            }
                        } else if (mouse.modifiers === Qt.NoModifier
                                   && groupStart.length > 1) {
                            // Pressing an already-selected tile keeps the set
                            // for a potential group drag; a plain click
                            // without movement collapses the selection to it.
                            root.selectOnly(tile)
                        }
                        groupStart = []
                        tile.dragged = false
                    }
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton)
                            root.openIconMenu(tile, mouse.x, mouse.y)
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton && !tile.dragged)
                            root.openEntry(tile.entryId)
                    }
                }

                ShellIcons.Icon {
                    objectName: "desktopIconsTileIcon"
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: String(tile.modelData.iconName) || "folder"
                    size: root.iconSize
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
                Accessible.name: String(modelData.accessibleName)
                Accessible.selected: tile.selected
                Accessible.onPressAction: root.openEntry(tile.entryId)
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
            onOpenRequested: root.openEntry(root.contextEntryId)
            onRenameRequested: renamePopup.begin(root.contextEntryId,
                                                  root.contextEntryLabel)
            onCutRequested: root.cutSelectionOps()
            onCopyRequested: root.copySelectionOps()
            onDeleteRequested: root.trashSelection()
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
