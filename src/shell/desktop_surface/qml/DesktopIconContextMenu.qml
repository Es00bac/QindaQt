// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Per-icon menu. Its 1x1 parent anchor is positioned by DesktopIconsView so
// the xdg popup appears beneath the physical pointer on Wayland. The menu
// opens with exactly one icon or a whole multi-selection targeted (the view
// selects on right-press): Cut, Copy, Move to Trash and Add to Dock act on the
// view's current selection, while Open, Open With, Get Info and Rename act on
// the icon that was right-clicked, as the view has always opened it.
//
// ADR-0273: desktop icons are one more File Manager view. Every entry but the
// dock's is a File Manager action, in the File Manager's order and in its own
// words, read from its public menu catalog by action id (DesktopFileActions).
// Open With and Get Info open File Manager's own dialogs.
//
// AGENT-NOTE: entries come from an Instantiator over a descriptor array, as in
// DesktopContextMenu: QQC2 Menu evicts an item for good once it is hidden, so
// rows that come and go (Open With for files only, the dock's row) are never
// gated with `visible`.
T.Menu {
    id: root
    signal openRequested()
    signal openWithRequested()
    signal infoRequested()
    signal renameRequested()
    signal cutRequested()
    signal copyRequested()
    signal deleteRequested()
    // ADR-0265: offered only where a dock is composed. A desktop entry reads
    // "Pin to Dock" (it becomes its installed application); anything else
    // is kept as a file or folder item.
    signal addToDockRequested()
    property bool dockAvailable: false
    property bool desktopEntry: false
    // The right-clicked icon is a folder, which has no Open With.
    property bool directory: false
    popupType: T.Popup.Window
    topPadding: 4
    bottomPadding: 4
    implicitWidth: Math.max(180, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(1, implicitContentHeight + topPadding + bottomPadding)
    contentItem: ListView {
        implicitWidth: 180
        implicitHeight: contentHeight
        model: root.contentModel
        currentIndex: root.currentIndex
    }
    background: Rectangle {
        color: "#ee20242a"
        radius: 6
        border.color: "#3c433f"
    }

    function dispatch(kind) {
        switch (kind) {
        case "open": root.openRequested(); break
        case "openWith": root.openWithRequested(); break
        case "info": root.infoRequested(); break
        case "rename": root.renameRequested(); break
        case "cut": root.cutRequested(); break
        case "copy": root.copyRequested(); break
        case "trash": root.deleteRequested(); break
        case "dock": root.addToDockRequested(); break
        }
    }

    function entry(objectName, actionId, kind) {
        return {objectName: objectName, text: DesktopFileActions.label(actionId), kind: kind}
    }

    readonly property var entries: {
        const rows = [root.entry("desktopIconContextOpen", "file.open", "open")]
        if (!root.directory)
            rows.push(root.entry("desktopIconContextOpenWith", "file.open-with", "openWith"))
        rows.push({separator: true},
                  root.entry("desktopIconContextGetInfo", "file.properties", "info"),
                  root.entry("desktopIconContextRename", "file.rename", "rename"),
                  {separator: true},
                  root.entry("desktopIconContextCut", "edit.cut", "cut"),
                  root.entry("desktopIconContextCopy", "edit.copy", "copy"),
                  {separator: true},
                  root.entry("desktopIconContextTrash", "file.trash", "trash"))
        if (root.dockAvailable) {
            rows.push({objectName: "desktopIconContextAddToDock",
                       text: root.desktopEntry ? qsTr("Pin to Dock") : qsTr("Add to Dock"),
                       kind: "dock"})
        }
        return rows
    }

    Instantiator {
        model: root.entries

        delegate: DesktopMenuItem {
            id: menuEntry

            onTriggered: root.dispatch(String(menuEntry.modelData.kind ?? ""))
        }

        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
}
