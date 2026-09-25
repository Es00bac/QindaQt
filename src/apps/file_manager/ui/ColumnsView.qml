// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk
import QindaQt.Tokens 1.0
import "EntryText.js" as EntryText

// Columns (ADR-0270), macOS's column view: the folders above the one being
// browsed (read-only FolderColumns from ColumnListing), the browsed folder
// itself -- NavigationController's listing, with the window's selection,
// rubber band, drag and drop, keyboard and context menu -- and a preview
// column for the current entry: a folder's contents, or a file's picture
// and key facts. Left and Right walk up and down the folder levels.
//
// Only a local folder has columns above it: a network location, search
// results' folder above the results aside, and Applications show the
// browsed column and the preview alone.
Control {
    id: root
    objectName: "columnsView"

    required property var navigationController
    required property var selection
    required property var appCoordinator
    property var mutationController: null
    property var clipboardController: null
    property var fileActions: null
    property var preferencesController: null
    property var entryFacts: null
    // ADR-0270: ColumnListing; null (fixtures) shows no columns above.
    property var columnListing: null
    // FolderViewSettings: a walk between folder levels stays in Columns.
    property var folderViews: null
    // True while this view is the one shown: nothing else is listed or
    // previewed for a hidden Columns view.
    property bool active: false

    property int iconSize: 64
    signal zoomRequested(int steps)

    readonly property bool showExtensions: preferencesController
        ? preferencesController.showExtensions === true : true
    readonly property int columnWidth: 180 + iconSize
    readonly property int rowIconSize: Math.max(16, Math.round(iconSize * 0.3125))
    readonly property int rowHeight: Math.max(rowIconSize + 10, Tokens.touch.rowHeight ?? 0)
    readonly property var current: root.selection.currentIndex >= 0
        ? (root.selection.entries[root.selection.currentIndex] || null) : null
    readonly property int maximumAncestors: 8

    // The browsed folder's ancestors, nearest last: [{path, childPath}].
    readonly property var ancestors: {
        const path = String(root.navigationController.currentPath || "")
        if (!root.active || root.columnListing === null || !path.startsWith("/") || path === "/")
            return []
        const out = []
        let child = path
        while (child !== "/" && out.length < root.maximumAncestors) {
            const slash = child.lastIndexOf("/")
            const parent = slash <= 0 ? "/" : child.substring(0, slash)
            out.unshift({ "path": parent, "childPath": child })
            child = parent
        }
        return out
    }

    function listing(path) {
        const navigation = root.navigationController
        return root.columnListing === null ? []
            : root.columnListing.children(path, navigation.showHidden === true,
                                          String(navigation.sortColumn || "name"),
                                          String(navigation.sortDirection || "ascending"),
                                          navigation.directoriesFirst !== false)
    }
    // Moves between folder levels without leaving Columns, even into a
    // folder that has a view of its own (Finder's column browsing).
    function walk(move) {
        if (root.folderViews)
            root.folderViews.keepViewModeOnce = true
        move()
        if (root.folderViews)
            root.folderViews.keepViewModeOnce = false
    }
    // A read-only row was clicked: show its folder, then select the row there.
    function openFrom(folder, row) {
        const navigation = root.navigationController
        root.walk(() => navigation.navigateTo(row.isDirectory === true ? row.path : folder))
        if (row.isDirectory !== true && typeof navigation.indexOfName === "function") {
            const index = navigation.indexOfName(row.name)
            if (index >= 0)
                root.selection.selectOnly(index)
        }
        root.focusView()
    }
    // Left: up one level, with the folder we came from selected.
    function goOut() {
        const navigation = root.navigationController
        const from = String(navigation.currentPath || "")
        if (navigation.canGoUp !== true)
            return
        root.walk(() => navigation.goUp())
        if (typeof navigation.indexOfName === "function") {
            const index = navigation.indexOfName(from.substring(from.lastIndexOf("/") + 1))
            if (index >= 0) {
                root.selection.selectOnly(index)
                root.revealIndex(index)
            }
        }
    }
    // Right: into the current folder, with its first entry selected.
    function goIn() {
        if (root.current === null || root.current.isDirectory !== true)
            return
        const folder = root.current.path
        root.walk(() => root.navigationController.navigateTo(folder))
        if (root.selection.entries.length > 0)
            root.selection.selectOnly(0)
    }

    readonly property alias focusItem: currentColumn.listView
    function focusView() { currentColumn.listView.forceActiveFocus() }
    function handleKey(event) { keyboardNavigation.handle(event) }
    function currentEntry() { return root.current }
    function selectedEntries() { return root.selection.selectedEntries() }
    function selectAll() { root.selection.selectAll() }
    function activateCurrent() {
        if (root.selection.currentIndex >= 0)
            root.navigationController.activate(root.selection.currentIndex)
    }
    function revealIndex(index) {
        currentColumn.listView.forceLayout()
        currentColumn.listView.positionViewAtIndex(index, ListView.Contain)
    }
    function popupFor(count) {
        contextMenu.selectionCount = count
        contextMenu.popup()
    }

    ViewportNavigation {
        id: keyboardNavigation
        view: currentColumn.listView
        selection: root.selection
        navigationController: root.navigationController
        columns: 1
        rowHeight: root.rowHeight
        onContextMenuRequested: {
            if (root.selection.currentIndex >= 0) {
                if (root.selection.selectedEntries().length === 0)
                    root.selection.selectOnly(root.selection.currentIndex)
                root.popupFor(root.selection.selectedEntries().length)
            } else {
                root.popupFor(0)
            }
        }
    }
    onIconSizeChanged: keyboardNavigation.scheduleReveal()

    Connections {
        target: root.navigationController
        // The browsed column is always the one in view after a move.
        function onNavigationChanged() { Qt.callLater(strip.showEnd) }
    }

    padding: 8

    contentItem: ColumnLayout {
        spacing: 0

        Flickable {
            id: strip
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: columns.width
            contentHeight: height
            interactive: contentWidth > width
            function showEnd() { strip.contentX = Math.max(0, strip.contentWidth - strip.width) }
            onWidthChanged: showEnd()
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

            Row {
                id: columns
                height: strip.height
                spacing: 1

                Repeater {
                    model: root.ancestors
                    delegate: FolderColumn {
                        required property var modelData
                        width: root.columnWidth
                        height: columns.height
                        rows: root.listing(modelData.path)
                        highlightPath: modelData.childPath
                        rowHeight: root.rowHeight
                        rowIconSize: root.rowIconSize
                        showExtensions: root.showExtensions
                        mutationController: root.mutationController
                        clipboardController: root.clipboardController
                        onRowActivated: (row) => root.openFrom(modelData.path, row)
                    }
                }

                // The browsed folder (NavigationController's listing).
                ColumnsCurrent {
                    id: currentColumn
                    view: root
                    width: root.columnWidth
                    height: columns.height
                }

                Loader {
                    id: preview
                    objectName: "columnsPreview"
                    width: root.columnWidth + 60
                    height: columns.height
                    active: root.active && root.current !== null
                    sourceComponent: root.current !== null && root.current.isDirectory === true
                        && String(root.current.path).startsWith("/") ? folderPreview : factsPreview
                }
            }
        }

        FileContextMenu {
            id: contextMenu
            objectName: "columnsContextMenu"
            appCoordinator: root.appCoordinator
            navigationController: root.navigationController
            clipboardController: root.clipboardController
            mutationController: root.mutationController
            fileActions: root.fileActions
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 4
            visible: root.navigationController.statusMessage.length > 0
            text: root.navigationController.statusMessage
            color: root.palette.placeholderText
        }
    }

    Component {
        id: folderPreview
        FolderColumn {
            rows: root.current !== null ? root.listing(root.current.path) : []
            rowHeight: root.rowHeight
            rowIconSize: root.rowIconSize
            showExtensions: root.showExtensions
            mutationController: root.mutationController
            clipboardController: root.clipboardController
            onRowActivated: (row) => root.openFrom(root.current.path, row)
        }
    }
    Component {
        id: factsPreview
        ColumnLayout {
            spacing: Tk.Theme.space.md
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(width, 240)
                Image {
                    id: previewIcon
                    anchors.centerIn: parent
                    width: Math.min(parent.width, parent.height) / 2
                    height: width
                    visible: !previewImage.real
                    sourceSize: Qt.size(width, height)
                    source: root.current ? EntryText.iconUrl(root.current, width) : ""
                }
                Image {
                    id: previewImage
                    readonly property bool real: status === Image.Ready && implicitWidth > 1
                    anchors.fill: parent
                    anchors.margins: Tk.Theme.space.md
                    visible: real
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                    source: root.current ? EntryText.galleryPreviewUrl(root.current) : ""
                }
            }
            EntryFactsPane {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Tk.Theme.space.md
                entry: root.current
                entryFacts: root.entryFacts
                relativeDates: root.preferencesController
                    ? root.preferencesController.relativeDates === true : false
                showExtensions: root.showExtensions
                applicationsPlace: root.navigationController.applicationsPlace === true
                locale: root.locale
            }
        }
    }
}
