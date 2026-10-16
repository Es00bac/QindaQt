// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// The context menu of one task-list tile: the same actions the Menu key, right
// click and touch context expose. It is created by the tile when asked for and
// destroyed after it closes (ADR-0396), because a Popup.Window keeps its window,
// and with it a GL context and video memory, for as long as the object exists.
//
// QindaQt.Controls ships no menu primitive yet (dock menu precedent); the menu
// uses the QQC2 style palette and owns no applet-side colors. It imports
// Controls and Tokens because the module's presentation gate requires both in
// every QML file.
T.Menu {
    id: menu

    // The TaskListEntryButton this menu acts for. Every action reads the tile's
    // current entry and the controller's revision when it is triggered
    // (ADR-0390), never a copy taken when the menu was built.
    required property var tile

    objectName: "taskListContextMenu"
    parent: tile
    popupType: T.Popup.Window

    T.MenuItem {
        objectName: "taskListContextActivate"
        text: qsTr("Activate")
        enabled: menu.tile.access !== null && menu.tile.access.canActivate
                 && !menu.tile.entry.pending
        onTriggered: menu.tile.dispatchActivate()
    }
    T.MenuItem {
        objectName: "taskListContextMinimize"
        text: menu.tile.entry.minimized ? qsTr("Restore") : qsTr("Minimize")
        enabled: menu.tile.access !== null && menu.tile.access.canManage
                 && !menu.tile.entry.pending
        onTriggered: menu.tile.access.minimizeTask(
                         menu.tile.entry.taskId, menu.tile.access.generationRevision)
    }
    T.MenuItem {
        objectName: "taskListContextClose"
        text: qsTr("Close")
        enabled: menu.tile.access !== null && menu.tile.access.canManage
                 && !menu.tile.entry.pending
        onTriggered: {
            if (menu.tile.targetWindowId.length > 0)
                menu.tile.access.closeTaskWindow(menu.tile.entry.taskId,
                                              menu.tile.targetWindowId,
                                              menu.tile.access.generationRevision)
            else
                menu.tile.access.closeTask(menu.tile.entry.taskId,
                                        menu.tile.access.generationRevision)
        }
    }
    T.MenuItem {
        objectName: "taskListContextRaise"
        text: qsTr("Raise")
        enabled: menu.tile.access !== null && menu.tile.access.canManage
                 && !menu.tile.entry.pending
        onTriggered: menu.tile.access.raiseTask(
                         menu.tile.entry.taskId, menu.tile.access.generationRevision)
    }
    T.MenuSeparator {
        visible: menu.tile.entry.kind === "container"
    }
    T.MenuItem {
        objectName: "taskListContextUngroup"
        visible: menu.tile.entry.kind === "container"
        text: qsTr("Ungroup")
        enabled: menu.tile.access !== null && menu.tile.access.canManage
                 && !menu.tile.entry.pending
        onTriggered: menu.tile.access.ungroupContainer(
                         menu.tile.entry.taskId, menu.tile.access.generationRevision)
    }
    // Keep in Dock (ADR-0265) pins this window's application; the dock
    // then shows it with a running indicator instead of this tile. The
    // dock answers when the menu opens, never from a stale binding.
    onAboutToShow: keepInDockItem.available = menu.tile.dockAccess !== null
        && menu.tile.entry.kind !== "container"
        && menu.tile.dockAccess.canKeepInDock(String(menu.tile.entry.applicationId ?? ""))
    T.MenuSeparator {
        visible: keepInDockItem.available
    }
    T.MenuItem {
        id: keepInDockItem
        property bool available: false
        objectName: "taskListContextKeepInDock"
        visible: available
        height: visible ? implicitHeight : 0
        text: qsTr("Keep in Dock")
        onTriggered: menu.tile.dockAccess.keepInDock(String(menu.tile.entry.applicationId ?? ""))
    }
    T.MenuSeparator {
        visible: menu.tile.dockMode && menu.tile.stripMove !== null
    }
    T.MenuItem {
        // Keyboard parity for the drag-reorder gesture (dock mode).
        objectName: "taskListContextMoveLeft"
        visible: menu.tile.dockMode && menu.tile.stripMove !== null
        text: qsTr("Move left")
        enabled: menu.tile.stripMove !== null && menu.tile.index > 0
        onTriggered: menu.tile.stripMove(menu.tile.index, -1)
    }
    T.MenuItem {
        objectName: "taskListContextMoveRight"
        visible: menu.tile.dockMode && menu.tile.stripMove !== null
        text: qsTr("Move right")
        enabled: menu.tile.stripMove !== null && menu.tile.access !== null
                 && menu.tile.index < menu.tile.access.entryCount - 1
        onTriggered: menu.tile.stripMove(menu.tile.index, 1)
    }
}
