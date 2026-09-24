// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T

// The context menu of one dock tile (ADR-0265). It carries only intents; the
// dock applet maps each to a facade call. Every drag gesture has a keyboard
// equivalent here: moving, removing, grouping, renaming, ungrouping.
//
// QindaQt.Controls ships no menu primitive yet (task-list precedent); the
// menu uses the QQC2 style palette and owns no applet colors.
T.Menu {
    id: menu

    // The tile's row (see QuickLaunchController::rows) and its position.
    property var row: ({})
    property int visualIndex: -1
    property int visualCount: 0
    property bool vertical: false
    property bool trashInDock: false
    // The tile the menu was opened from; follow-up popups anchor to it.
    property Item anchorTile: null

    readonly property string kind: String(row.kind ?? "")
    // A permanent end (ADR-0268) is never grouped, moved, or removed.
    readonly property bool fixed: Boolean(row.fixed)

    signal openRequested()
    signal openNewWindowRequested()
    signal openInFileManagerRequested()
    signal emptyTrashRequested()
    signal newGroupRequested()
    signal renameRequested()
    signal ungroupRequested()
    signal moveRequested(int direction)
    signal removeRequested()
    signal showTrashRequested()

    objectName: "quickLaunchContextMenu"
    popupType: T.Popup.Window

    component DockMenuItem: T.MenuItem {
        height: visible ? implicitHeight : 0
    }

    DockMenuItem {
        objectName: "quickLaunchOpen"
        text: qsTr("Open")
        onTriggered: menu.openRequested()
    }
    DockMenuItem {
        objectName: "quickLaunchOpenNewWindow"
        visible: menu.kind === "application" && Boolean(menu.row.running)
        text: qsTr("Open New Window")
        onTriggered: menu.openNewWindowRequested()
    }
    DockMenuItem {
        objectName: "quickLaunchOpenInFileManager"
        visible: menu.kind === "folder" || menu.kind === "trash"
        text: qsTr("Open in File Manager")
        onTriggered: menu.openInFileManagerRequested()
    }
    DockMenuItem {
        objectName: "quickLaunchEmptyTrash"
        visible: menu.kind === "trash"
        text: qsTr("Empty Trash…")
        onTriggered: menu.emptyTrashRequested()
    }
    T.MenuSeparator {
        visible: !menu.fixed
    }
    DockMenuItem {
        objectName: "quickLaunchNewGroup"
        visible: menu.kind === "application" && !menu.fixed
        text: qsTr("New Group…")
        onTriggered: menu.newGroupRequested()
    }
    DockMenuItem {
        objectName: "quickLaunchRenameGroup"
        visible: menu.kind === "group"
        text: qsTr("Rename Group…")
        onTriggered: menu.renameRequested()
    }
    DockMenuItem {
        objectName: "quickLaunchUngroup"
        visible: menu.kind === "group"
        text: qsTr("Ungroup")
        onTriggered: menu.ungroupRequested()
    }
    DockMenuItem {
        objectName: "quickLaunchMoveUp"
        visible: !menu.fixed
        text: menu.vertical ? qsTr("Move up") : qsTr("Move left")
        enabled: menu.visualIndex > 0
        onTriggered: menu.moveRequested(-1)
    }
    DockMenuItem {
        objectName: "quickLaunchMoveDown"
        visible: !menu.fixed
        text: menu.vertical ? qsTr("Move down") : qsTr("Move right")
        enabled: menu.visualIndex >= 0 && menu.visualIndex < menu.visualCount - 1
        onTriggered: menu.moveRequested(1)
    }
    DockMenuItem {
        objectName: "quickLaunchUnpin"
        visible: !menu.fixed
        text: qsTr("Remove from Dock")
        onTriggered: menu.removeRequested()
    }
    T.MenuSeparator {
        visible: !menu.trashInDock
    }
    DockMenuItem {
        objectName: "quickLaunchShowTrash"
        visible: !menu.trashInDock
        text: qsTr("Show Trash in Dock")
        onTriggered: menu.showTrashRequested()
    }
}
