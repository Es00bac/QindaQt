// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk
import QindaQt.Tokens 1.0
import "EntryDrag.js" as EntryDrag
import QindaQt.Controls 1.0 as C

// Details (ADR-0270): the listing as a virtualised Tk.DataTable -- sortable,
// resizable columns chosen and ordered per folder (ColumnChooser), Group By
// headings, relative dates, compact rows and hidden extensions.
//
// AGENT-NOTE: Tk.DataTable (r5) models one current row; the window's
// multi-selection, rubber band, drag and drop and keyboard stay the shared
// EntrySelection/ViewportNavigation ones. The table's model is rowModel.rows
// (entry indexes plus {heading} rows), its current row stays -1, and the Name
// cell (DetailsNameCell) carries each row's behaviour. The table keeps its
// row ListView to itself, so bodyList is found once through the documented
// "tableBody" Scroll; the rubber band and the scroll bar attach to it.
Control {
    id: root
    objectName: "detailsView"

    required property var navigationController
    required property var selection
    required property var appCoordinator
    // Optional; Main passes the real ones and fixture views may leave them
    // null (drops refuse, columns fall back to the defaults, facts stay dashes).
    property var mutationController: null
    property var clipboardController: null
    property var fileActions: null
    property var folderViews: null
    property var preferencesController: null
    property var entryFacts: null

    property int iconSize: 64
    signal zoomRequested(int steps)

    readonly property bool applicationsPlace: navigationController.applicationsPlace === true
    readonly property bool showExtensions: preferencesController
        ? preferencesController.showExtensions === true : true
    readonly property bool relativeDates: preferencesController
        ? preferencesController.relativeDates === true : false
    readonly property bool compact: preferencesController
        ? preferencesController.rowDensity === "compact" : false
    readonly property int rowIconSize: Math.max(compact ? 16 : 20,
                                                Math.round(iconSize * (compact ? 0.3125 : 0.4375)))
    // Rows grow to the touch row height while the last input was a finger (ADR-0193).
    readonly property int rowHeight: Math.max(rowIconSize + (compact ? 6 : 16),
                                              Tokens.touch.rowHeight ?? 0)
    readonly property real tableWidth: table.width
    readonly property bool keyboardFocused: keyTarget.activeFocus
    property var bodyList: null

    // Headings come from Group By, or (ADR-0262) the Applications place's
    // Group by Category, which is its category sort.
    readonly property string headingSource: {
        const group = navigationController.groupBy
        if (group !== undefined && group !== null && group !== "none")
            return "group"
        return applicationsPlace && navigationController.sortColumn === "kind" ? "kind" : ""
    }
    // The table's rows, and the row that shows each entry.
    readonly property var rowModel: {
        const entries = root.selection.entries
        const rows = []
        const rowOf = []
        let last = null
        for (let i = 0; i < entries.length; ++i) {
            if (root.headingSource.length > 0) {
                const label = String((root.headingSource === "group" ? entries[i].group
                                                                     : entries[i].kindText) || "")
                if (label !== last) {
                    rows.push({ "heading": label })
                    last = label
                }
            }
            rowOf.push(rows.length)
            rows.push(i)
        }
        return { "rows": rows, "rowOf": rowOf }
    }
    readonly property int headingCount: rowModel.rows.length - rowModel.rowOf.length

    readonly property alias focusItem: keyTarget
    function focusView() { keyTarget.forceActiveFocus() }
    function entryAt(index) { return root.selection.entries[index] || null }
    function currentEntry() { return root.entryAt(root.selection.currentIndex) }
    function selectedEntries() { return root.selection.selectedEntries() }
    function selectAll() { root.selection.selectAll() }
    function activateCurrent() {
        if (root.selection.currentIndex >= 0)
            root.navigationController.activate(root.selection.currentIndex)
    }
    function revealIndex(index) {
        const row = root.rowModel.rowOf[index]
        if (root.bodyList === null || row === undefined)
            return
        root.bodyList.forceLayout()
        root.bodyList.positionViewAtIndex(row, ListView.Contain)
    }
    function popupFor(count) {
        contextMenu.selectionCount = count
        contextMenu.popup()
    }
    function openColumnChooser() {
        chooser.anchorItem = headerStrip
        chooser.open()
    }

    // A header click asks for (key, order); NavigationController reverses
    // the active column instead of taking an order, so this asks it twice at
    // most. A fixture navigation without the method just ignores it.
    function sortBy(key, order) {
        const navigation = root.navigationController
        if (navigation.sortColumn !== key)
            navigation.setSortColumn(key)
        if ((navigation.sortDirection === "descending") !== (order === Qt.DescendingOrder))
            navigation.setSortColumn(key)
    }

    // AGENT-NOTE: see the header note; a missing body only disables the
    // rubber band, the scroll bar and reveal, never the table itself.
    function findNamed(item, name) {
        if (item.objectName === name)
            return item
        for (let i = 0; i < item.children.length; ++i) {
            const found = root.findNamed(item.children[i], name)
            if (found)
                return found
        }
        return null
    }
    Component.onCompleted: {
        const body = root.findNamed(table, "tableBody")
        const hosted = body && body.contentItem ? body.contentItem.children : []
        for (let i = 0; i < hosted.length; ++i) {
            if (typeof hosted[i].positionViewAtIndex === "function")
                root.bodyList = hosted[i]
        }
    }

    ViewportNavigation {
        id: keyboardNavigation
        view: keyTarget
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

    DetailsColumnSet {
        id: columnSet
        objectName: "detailsColumnSet"
        view: root
    }

    padding: 8

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true

            DropArea {
                anchors.fill: parent
                onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                onDropped: (drop) => {
                    const action = EntryDrag.dispatch(
                        drop, root.navigationController.currentPath,
                        root.mutationController, root.clipboardController)
                    if (action !== Qt.IgnoreAction)
                        drop.accept(action)
                    else
                        drop.accepted = false
                }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: {
                    root.focusView()
                    root.popupFor(0)
                }
            }
            C.TouchContextArea {
                objectName: "entryListTouchContext"
                anchors.fill: parent
                onContextRequested: {
                    root.focusView()
                    root.popupFor(0)
                }
            }

            // AGENT-CONTRACT: "entryListView" is the Details view's keyboard
            // focus and the probes' and tests' handle (selectEntry(),
            // currentIndex, count, rowHeight), placed exactly over the rows.
            Item {
                id: keyTarget
                objectName: "entryListView"
                x: table.x
                y: table.y + Tk.Theme.size.header
                width: table.width
                height: Math.max(0, table.height - Tk.Theme.size.header)
                focus: true
                readonly property int currentIndex: root.selection.currentIndex
                readonly property int count: root.selection.entries.length
                readonly property int rowHeight: root.rowHeight
                function selectEntry(index) { root.selection.selectOnly(index) }
                function revealIndex(index) { root.revealIndex(index) }
                onWidthChanged: keyboardNavigation.scheduleReveal()
                onHeightChanged: keyboardNavigation.scheduleReveal()
                onVisibleChanged: if (visible) keyboardNavigation.scheduleReveal()
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Folder contents")
                Keys.onReturnPressed: root.activateCurrent()
                Keys.onEnterPressed: root.activateCurrent()
                Keys.onPressed: (event) => keyboardNavigation.handle(event)
            }

            Tk.DataTable {
                id: table
                objectName: "detailsTable"
                anchors.fill: parent
                anchors.rightMargin: 16
                activeFocusOnTab: false
                model: root.rowModel.rows
                columns: columnSet.ordered(columnSet.shownKeys,
                                           root.navigationController.guestListingActive === true)
                sortKey: root.navigationController.sortColumn
                sortOrder: root.navigationController.sortDirection === "descending"
                    ? Qt.DescendingOrder : Qt.AscendingOrder
                currentIndex: -1
                alternatingRows: true
                rowHeight: root.rowHeight
                onSortRequested: (key, order) => root.sortBy(key, order)
                onColumnResized: (key, width) => columnSet.resizeColumn(key, width)
            }
            // The table writes its own map while a seam is dragged; this
            // re-applies the folder's widths whenever they change.
            Binding {
                target: table
                property: "columnWidths"
                value: columnSet.widthMap
            }

            // Right-click the header to choose columns (Finder's way).
            MouseArea {
                id: headerStrip
                x: table.x
                y: table.y
                width: table.width
                height: Tk.Theme.size.header
                acceptedButtons: Qt.RightButton
                onClicked: root.openColumnChooser()
            }
            // Ctrl+wheel zooms; a plain wheel reaches the rows and scrolls.
            Item {
                x: keyTarget.x
                y: keyTarget.y
                width: keyTarget.width
                height: keyTarget.height
                ZoomWheelHandler { onZoomRequested: (steps) => root.zoomRequested(steps) }
            }

            SelectionBand {
                parent: root.bodyList
                anchors.fill: parent
                z: 10
                view: root.bodyList
                mapIndex: function(row) {
                    const index = root.rowModel.rows[row]
                    return typeof index === "number" ? index : -1
                }
                onFinished: (indexes, modifiers) => root.selection.applyIndexSet(indexes, modifiers)
            }

            ViewportScrollBar {
                id: scrollBar
                objectName: "entryListScrollBar"
                x: table.width + 4
                y: keyTarget.y
                height: keyTarget.height
                size: root.bodyList ? root.bodyList.visibleArea.heightRatio : 1
                position: root.bodyList ? root.bodyList.visibleArea.yPosition : 0
                onPositionChanged: {
                    if (pressed && root.bodyList)
                        root.bodyList.contentY = root.bodyList.originY
                            + position * root.bodyList.contentHeight
                }
            }
        }

        FileContextMenu {
            id: contextMenu
            objectName: "listContextMenu"
            appCoordinator: root.appCoordinator
            navigationController: root.navigationController
            clipboardController: root.clipboardController
            mutationController: root.mutationController
            fileActions: root.fileActions
        }

        ColumnChooser {
            id: chooser
            view: root
            columnSet: columnSet
            navigationController: root.navigationController
            preferencesController: root.preferencesController
            folderViews: root.folderViews
        }

        Label {
            objectName: "truncationNotice"
            Layout.fillWidth: true
            Layout.topMargin: 4
            visible: root.navigationController.statusMessage.length > 0
            text: root.navigationController.statusMessage
            color: root.palette.placeholderText
        }
    }
}
