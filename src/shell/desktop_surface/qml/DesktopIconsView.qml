// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons

// Desktop-directory icons with free placement, persistent per-output
// geometry, activation, and an item-specific context menu.
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
             : Math.round(Math.min(96, Math.max(24, requested)))
    }
    readonly property real tileWidth: Math.max(iconSize + 24, 104)
    readonly property real tileHeight: iconSize + 40
    readonly property var rows: contents.rows
    property string selectedId: ""
    property string contextEntryId: ""
    property string contextEntryLabel: ""

    function clearSelection() { selectedId = "" }
    function openEntry(entryId) { contents.open(entryId) }
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
            if (tile !== null) restoreTile(tile)
        }
    }
    function reflow() {
        selectedId = ""
        layoutStore.clearScreen(screenName)
        contents.refresh()
        Qt.callLater(restoreAllTiles)
    }
    function positionAnchor(anchor, pointerX, pointerY, popupWidth, popupHeight) {
        const x = Math.max(0, Math.min(pointerX, Math.max(0, width - popupWidth)))
        const y = Math.max(0, Math.min(pointerY, Math.max(0, height - popupHeight)))
        anchor.x = Math.max(0, x - anchor.width)
        anchor.y = y
    }
    function openIconMenu(tile, localX, localY) {
        selectedId = tile.entryId
        contextEntryId = tile.entryId
        contextEntryLabel = tile.entryLabel
        const point = tile.mapToItem(root, localX, localY)
        positionAnchor(iconMenuAnchor, point.x, point.y,
                       iconContextMenu.width, iconContextMenu.height)
        iconContextMenu.popup()
    }

    onWidthChanged: Qt.callLater(restoreAllTiles)
    onHeightChanged: Qt.callLater(restoreAllTiles)
    Component.onCompleted: Qt.callLater(restoreAllTiles)

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
                readonly property bool selected: root.selectedId === entryId
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
                    cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
                    drag.target: tile
                    drag.minimumX: 0
                    drag.maximumX: Math.max(0, root.width - tile.width)
                    drag.minimumY: 0
                    drag.maximumY: Math.max(0, root.height - tile.height)
                    onPressed: (mouse) => {
                        tile.dragged = false
                        if (mouse.button === Qt.LeftButton) {
                            root.selectedId = tile.entryId
                            tile.forceActiveFocus(Qt.MouseFocusReason)
                        }
                    }
                    onPositionChanged: { if (drag.active) tile.dragged = true }
                    onReleased: (mouse) => {
                        if (mouse.button === Qt.LeftButton && tile.dragged) {
                            root.layoutStore.setPosition(root.screenName,
                                                         tile.layoutKey,
                                                         tile.x, tile.y)
                        }
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
                Accessible.name: String(tile.modelData.accessibleName)
                Accessible.selected: tile.selected
                Accessible.onPressAction: root.openEntry(tile.entryId)
            }
        }
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
