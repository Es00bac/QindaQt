// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Styled desktop context menu (ADR-0125). One menu, three item sets driven by
// the desktop-icons `contextMenuStyle` setting. Every entry dispatches
// through an existing seam: launcherAccess.activate(entryId, actionId),
// placesAccess.open(placeId), newFolder.create(), customizationAccess.enterEditMode()
// (Edit Panels, ADR-0266), or the icons view (reflow,
// refresh, paste, select all, and File Manager actions through its contents
// controller). A null facade disables its entries instead of crashing.
//
// ADR-0273: the desktop is one more File Manager view. Its folder actions
// (New Folder, New File, Paste, Select All, Get Info, Refresh, Open) are in
// every style and use the File Manager's own words, read from its public
// menu catalog by action id (DesktopFileActions); only the desktop's own
// entries (Arrange, Sort By, Clean Up, settings) spell their labels here.
//
// AGENT-GUARD: the popup must keep popupType Window (panel precedent). The
// desktop surface is a focus-less layer-shell toplevel; an Item-popup child
// would inherit a window that can never take keyboard focus.
//
// AGENT-NOTE: entries come from an Instantiator over a per-style descriptor
// array, never from `visible` gating on a fixed item set. QQC2 Menu
// permanently evicts an item from its content model once hidden (and severs
// initially-false visible bindings — Qt 6.11, both popup types), while
// createObject()'d rows trip "not placed in the graphics scene" under
// QT_FATAL_WARNINGS. Instantiator + insertItem/removeItem is the documented
// dynamic-menu path; separators ride the same delegate as zero-height rows.
T.Menu {
    id: root

    property string style: "windows"
    // Borrowed PlacesController facade; may be null.
    property var placesAccess: null
    // Borrowed LauncherAppletController facade; may be null.
    property var launcherAccess: null
    property NewFolderController newFolder: null
    property var iconsView: null
    // Borrowed LiveCustomizationController facade; may be null. Backs Edit
    // Panels (ADR-0266).
    property var customizationAccess: null

    signal applicationsRequested()

    popupType: T.Popup.Window
    objectName: "desktopContextMenu"
    topPadding: 4
    bottomPadding: 4

    // Templates.Menu supplies behavior but no style-owned sizing or content
    // view. A Window popup must have both dimensions before its first Wayland
    // commit; otherwise KWin disconnects the whole shell for the invalid 0x0
    // surface. Keep this equivalent to the sizing contract a QQC2 style
    // normally contributes while retaining the explicitly owned appearance.
    //
    // AGENT-GUARD: never remove the positive implicit-size floor or the
    // contentModel-backed ListView. Offscreen QML can report a zero-sized
    // Menu as opened, but a real Wayland compositor rejects that surface.
    implicitWidth: Math.max(200,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(1,
                             implicitContentHeight + topPadding + bottomPadding)

    contentItem: ListView {
        implicitWidth: 200
        implicitHeight: contentHeight
        model: root.contentModel
        currentIndex: root.currentIndex
        interactive: contentHeight + root.topPadding + root.bottomPadding
                     > root.height
        clip: true
    }

    // Instantiated from Templates (project precedent — Controls ships no menu
    // primitive), so the menu owns its surface and palette instead of relying
    // on an ambient QQC2 style or app palette.
    background: Rectangle {
        color: "#ee20242a"
        radius: 6
        border.color: "#3c433f"
    }
    palette.window: "#20242a"
    palette.windowText: "#ffeeeeee"
    palette.text: "#ffeeeeee"
    palette.highlight: "#3b74dd"
    palette.highlightedText: "#ffffff"

    function reflowIcons() {
        if (iconsView !== null) {
            iconsView.reflow()
        }
    }

    function activate(entryId, actionId) {
        if (launcherAccess !== null) {
            launcherAccess.activate(entryId, actionId)
        }
    }

    function createFolder() {
        if (newFolder === null) {
            return
        }
        const created = newFolder.create()
        if (created.length > 0) {
            reflowIcons()
        }
    }

    function dispatch(kind, targetId, actionId) {
        switch (kind) {
        case "reflow":
            reflowIcons()
            break
        case "newFolder":
            createFolder()
            break
        case "paste":
            if (iconsView !== null) {
                iconsView.pasteClipboard()
            }
            break
        case "refresh":
            if (iconsView !== null) {
                iconsView.contents.refresh()
            }
            break
        case "selectAll":
            if (iconsView !== null) {
                iconsView.selectAll()
            }
            break
        case "fileManager":
            if (iconsView !== null) {
                iconsView.runFileManagerAction(actionId, "")
            }
            break
        case "launch":
            activate(targetId, actionId)
            break
        case "openPlace":
            if (placesAccess !== null) {
                placesAccess.open(targetId)
            }
            break
        case "applications":
            applicationsRequested()
            break
        case "editPanels":
            if (customizationAccess !== null) {
                customizationAccess.enterEditMode()
            }
            break
        }
    }

    // Facade gating for one descriptor: a null facade disables its entries.
    function entryEnabled(entry) {
        if (entry.needsLauncher === true) {
            return launcherAccess !== null
        }
        if (entry.needsNewFolder === true) {
            return newFolder !== null
        }
        if (entry.needsPlaces === true) {
            return placesAccess !== null
        }
        if (entry.needsClipboard === true) {
            return iconsView !== null && iconsView.canPaste === true
        }
        if (entry.needsView === true) {
            return iconsView !== null
        }
        if (entry.needsCustomization === true) {
            return customizationAccess !== null && customizationAccess.available === true
        }
        return true
    }

    // The File Manager's label for a catalog action id (ADR-0273).
    function fm(actionId) {
        return DesktopFileActions.label(actionId)
    }

    // One descriptor per visible row; `separator` rows render a divider line.
    readonly property var entries: {
        const newFolder = {objectName: "desktopContextNewFolder", text: root.fm("file.new-folder"),
                           kind: "newFolder", needsNewFolder: true}
        const newFile = {objectName: "desktopContextNewFile", text: root.fm("file.new-file"),
                         kind: "fileManager", actionId: "file.new-file", needsView: true}
        const paste = {objectName: "desktopContextPaste", text: root.fm("edit.paste"),
                       kind: "paste", needsClipboard: true}
        const selectAll = {objectName: "desktopContextSelectAll", text: root.fm("edit.select-all"),
                           kind: "selectAll", needsView: true}
        const getInfo = {objectName: "desktopContextGetInfo", text: root.fm("file.properties"),
                         kind: "fileManager", actionId: "file.properties", needsView: true}
        if (style === "mac") {
            return [
                newFolder,
                newFile,
                {objectName: "desktopContextOpen", text: root.fm("file.open"),
                 kind: "openPlace", targetId: "desktop", needsPlaces: true},
                getInfo,
                {separator: true},
                paste,
                selectAll,
                {separator: true},
                {objectName: "desktopContextSortBy", text: qsTr("Sort By"),
                 kind: "reflow"},
                {objectName: "desktopContextCleanUp", text: qsTr("Clean Up"),
                 kind: "reflow"},
                {separator: true},
                {objectName: "desktopContextScreenSaver",
                 text: qsTr("Change Desktop && Screen Saver…"),
                 kind: "launch", targetId: "org.qindaqt.Settings",
                 actionId: "appearance", needsLauncher: true},
                {objectName: "desktopContextEditPanels", text: qsTr("Edit Panels"),
                 kind: "editPanels", needsCustomization: true},
            ]
        }
        if (style === "traditional") {
            return [
                {objectName: "desktopContextApplications",
                 text: qsTr("Applications"), kind: "applications"},
                {separator: true},
                {objectName: "desktopContextTerminal",
                 text: qsTr("Open Terminal Here"),
                 kind: "launch", targetId: "org.qindaqt.QQTerm",
                 needsLauncher: true},
                newFolder,
                newFile,
                paste,
                selectAll,
                {separator: true},
                getInfo,
                {objectName: "desktopContextSettings",
                 text: qsTr("Desktop Settings"),
                 kind: "launch", targetId: "org.qindaqt.Settings",
                 actionId: "appearance", needsLauncher: true},
                {objectName: "desktopContextEditPanels", text: qsTr("Edit Panels"),
                 kind: "editPanels", needsCustomization: true},
            ]
        }
        return [
            {objectName: "desktopContextArrange", text: qsTr("Arrange Icons"),
             kind: "reflow"},
            // The File Manager's Refresh reads the folder again; arranging
            // stays with Arrange Icons.
            {objectName: "desktopContextRefresh", text: root.fm("view.refresh"),
             kind: "refresh", needsView: true},
            {separator: true},
            newFolder,
            newFile,
            paste,
            selectAll,
            {separator: true},
            getInfo,
            {objectName: "desktopContextDisplayProperties",
             text: qsTr("Display Properties"),
             kind: "launch", targetId: "org.qindaqt.Settings",
             actionId: "display", needsLauncher: true},
            {objectName: "desktopContextEditPanels", text: qsTr("Edit Panels"),
             kind: "editPanels", needsCustomization: true},
        ]
    }

    Instantiator {
        id: itemInstantiator

        model: root.entries

        delegate: DesktopMenuItem {
            id: menuEntry

            available: root.entryEnabled(menuEntry.modelData)
            onTriggered: root.dispatch(String(menuEntry.modelData.kind ?? ""),
                                       String(menuEntry.modelData.targetId ?? ""),
                                       String(menuEntry.modelData.actionId ?? ""))
        }

        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
}
