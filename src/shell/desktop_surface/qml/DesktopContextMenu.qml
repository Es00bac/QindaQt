// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Styled desktop context menu (ADR-0125). One menu, three item sets driven by
// the desktop-icons `contextMenuStyle` setting. Every entry dispatches
// through an existing seam: launcherAccess.activate(entryId),
// placesAccess.open(placeId), newFolder.create(), or the icons view reflow.
// A null facade disables its entries instead of crashing.
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

    signal applicationsRequested()

    popupType: T.Popup.Window
    objectName: "desktopContextMenu"
    topPadding: 4
    bottomPadding: 4

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

    function activate(entryId) {
        if (launcherAccess !== null) {
            launcherAccess.activate(entryId)
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

    function dispatch(kind, targetId) {
        switch (kind) {
        case "reflow":
            reflowIcons()
            break
        case "newFolder":
            createFolder()
            break
        case "launch":
            activate(targetId)
            break
        case "openPlace":
            if (placesAccess !== null) {
                placesAccess.open(targetId)
            }
            break
        case "applications":
            applicationsRequested()
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
        return true
    }

    // One descriptor per visible row; `separator` rows render a divider line.
    readonly property var entries: {
        if (style === "mac") {
            return [
                {objectName: "desktopContextNewFolder", text: qsTr("New Folder"),
                 kind: "newFolder", needsNewFolder: true},
                {objectName: "desktopContextOpen", text: qsTr("Open"),
                 kind: "openPlace", targetId: "desktop", needsPlaces: true},
                {separator: true},
                {objectName: "desktopContextSortBy", text: qsTr("Sort By"),
                 kind: "reflow"},
                {objectName: "desktopContextCleanUp", text: qsTr("Clean Up"),
                 kind: "reflow"},
                {separator: true},
                {objectName: "desktopContextScreenSaver",
                 text: qsTr("Change Desktop && Screen Saver…"),
                 kind: "launch", targetId: "org.qindaqt.Settings",
                 needsLauncher: true},
            ]
        }
        if (style === "traditional") {
            return [
                {objectName: "desktopContextApplications",
                 text: qsTr("Applications"), kind: "applications"},
                {separator: true},
                {objectName: "desktopContextTerminal",
                 text: qsTr("Open Terminal Here"),
                 kind: "launch", targetId: "org.qindaqt.Terminal",
                 needsLauncher: true},
                {objectName: "desktopContextCreateFolder",
                 text: qsTr("Create Folder…"), kind: "newFolder",
                 needsNewFolder: true},
                {separator: true},
                {objectName: "desktopContextSettings",
                 text: qsTr("Desktop Settings"),
                 kind: "launch", targetId: "org.qindaqt.Settings",
                 needsLauncher: true},
            ]
        }
        return [
            {objectName: "desktopContextArrange", text: qsTr("Arrange Icons"),
             kind: "reflow"},
            {objectName: "desktopContextRefresh", text: qsTr("Refresh"),
             kind: "reflow"},
            {separator: true},
            {objectName: "desktopContextNewFolder", text: qsTr("New Folder"),
             kind: "newFolder", needsNewFolder: true},
            {separator: true},
            {objectName: "desktopContextDisplayProperties",
             text: qsTr("Display Properties"),
             kind: "launch", targetId: "org.qindaqt.Settings",
             needsLauncher: true},
        ]
    }

    Instantiator {
        id: itemInstantiator

        model: root.entries

        delegate: T.MenuItem {
            id: entry

            required property var modelData
            required property int index

            readonly property bool isSeparator: modelData.separator === true

            objectName: isSeparator ? "" : String(modelData.objectName)
            enabled: !isSeparator && root.entryEnabled(modelData)
            hoverEnabled: !isSeparator
            padding: isSeparator ? 2 : 6
            leftPadding: isSeparator ? 2 : 10
            rightPadding: isSeparator ? 2 : 10

            onTriggered: root.dispatch(String(modelData.kind ?? ""),
                                       String(modelData.targetId ?? ""))

            contentItem: Item {
                implicitWidth: entry.isSeparator ? 160
                                                 : labelText.implicitWidth
                implicitHeight: entry.isSeparator ? 1
                                                  : labelText.implicitHeight

                Rectangle {
                    anchors.fill: parent
                    visible: entry.isSeparator
                    color: "#3c433f"
                }

                Text {
                    id: labelText
                    visible: !entry.isSeparator
                    text: String(entry.modelData.text ?? "")
                    color: entry.enabled ? "#ffeeeeee" : "#7f8a8a8a"
                    font: entry.font
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
            background: Rectangle {
                visible: !entry.isSeparator
                radius: 3
                color: entry.enabled
                       && (entry.hovered || entry.activeFocus)
                       ? "#3b74dd" : "transparent"
            }

            Accessible.role: entry.isSeparator ? Accessible.NoRole
                                               : Accessible.MenuItem
            Accessible.name: entry.isSeparator ? "" : String(modelData.text)
        }

        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
}
