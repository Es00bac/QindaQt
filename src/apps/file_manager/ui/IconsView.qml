// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "EntryDrag.js" as EntryDrag
import QindaQt.Controls 1.0 as C

// Icons (ADR-0270): a grid of Tk.Thumbnail tiles (IconTile.qml). Selection,
// keyboard, rubber band, drag and drop and the context menu are the window's
// shared ones, exactly as in the other three views.
Control {
    id: root

    required property var navigationController
    required property var selection
    required property var appCoordinator
    // Optional drop dispatch targets; Main always passes the real controllers,
    // fixture tests may leave them null (drops then refuse politely).
    property var mutationController: null
    property var clipboardController: null
    // ADR-0269: the window's FileActions, for the context menu's right-click set.
    property var fileActions: null
    // ADR-0270: Show filename extensions lives in the preferences.
    property bool showExtensions: true

    property int iconSize: 64
    signal zoomRequested(int steps)

    ViewportNavigation {
        id: keyboardNavigation
        view: gridView
        selection: root.selection
        navigationController: root.navigationController
        columns: Math.max(1, Math.floor(gridView.width / gridView.cellWidth))
        rowHeight: gridView.cellHeight
        // Keyboard invocation (Menu key / Shift+F10) follows the focused
        // item: with a valid current entry, select it first if nothing is
        // already selected (mirroring the mouse-click behavior below), then
        // target that selection; with no current entry (an empty folder),
        // target the background instead.
        onContextMenuRequested: {
            if (root.selection.currentIndex >= 0) {
                if (root.selection.selectedEntries().length === 0)
                    root.selection.selectOnly(root.selection.currentIndex)
                contextMenu.selectionCount = root.selection.selectedEntries().length
            } else {
                contextMenu.selectionCount = 0
            }
            contextMenu.popup()
        }
    }
    onIconSizeChanged: keyboardNavigation.scheduleReveal()

    readonly property alias focusItem: gridView

    function focusView() { gridView.forceActiveFocus() }

    function currentEntry() {
        return gridView.currentIndex >= 0 ? root.selection.entries[gridView.currentIndex] : null
    }

    function selectedEntries() {
        return selection.selectedEntries()
    }

    function selectAll() {
        selection.selectAll()
    }

    function activateCurrent() {
        if (gridView.currentIndex >= 0) {
            root.navigationController.activate(gridView.currentIndex)
        }
    }

    function revealIndex(index) {
        gridView.forceLayout()
        gridView.positionViewAtIndex(index, GridView.Contain)
    }

    function popupFor(count) {
        contextMenu.selectionCount = count
        contextMenu.popup()
    }

    padding: 8

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Background drop target: dropping on empty view space drops into
            // the browsed folder. Folder delegates carry their own DropArea,
            // which sits above this one and wins when both cover the cursor.
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

            // Background right-click: GridView/Flickable only claims
            // Qt.LeftButton by default, so a right-click that misses every
            // delegate's own MouseArea falls through to this one.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: {
                    gridView.forceActiveFocus()
                    root.popupFor(0)
                }
            }

            C.TouchContextArea {
                objectName: "entryGridTouchContext"
                anchors.fill: parent
                onContextRequested: {
                    gridView.forceActiveFocus()
                    root.popupFor(0)
                }
            }

            GridView {
                id: gridView
                objectName: "entryGridView"
                anchors.fill: parent
                anchors.rightMargin: 16
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                onWidthChanged: keyboardNavigation.scheduleReveal()
                onHeightChanged: keyboardNavigation.scheduleReveal()
                onVisibleChanged: if (visible) keyboardNavigation.scheduleReveal()
                focus: true
                keyNavigationEnabled: false
                currentIndex: root.selection.currentIndex
                function selectEntry(index) { root.selection.selectOnly(index) }
                cellWidth: Math.max(1, width / Math.max(1, Math.floor(width / (root.iconSize + 72))))
                cellHeight: root.iconSize + 80
                model: root.selection.entries

                ScrollBar.vertical: ViewportScrollBar {
                    objectName: "entryGridScrollBar"
                    parent: viewport
                    x: gridView.width + 4
                    height: gridView.height
                }
                ZoomWheelHandler { onZoomRequested: (steps) => root.zoomRequested(steps) }

                // Marquee selection over empty viewport space; the band
                // declines presses that land on a delegate so ordinary
                // clicking below is untouched.
                SelectionBand {
                    id: selectionBand
                    anchors.fill: parent
                    view: gridView
                    onFinished: (indexes, modifiers) =>
                        root.selection.applyIndexSet(indexes, modifiers)
                }

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Folder contents")

                Keys.onReturnPressed: root.activateCurrent()
                Keys.onEnterPressed: root.activateCurrent()
                Keys.onPressed: (event) => keyboardNavigation.handle(event)

                delegate: IconTile {
                    selection: root.selection
                    view: gridView
                    iconSize: root.iconSize
                    showExtensions: root.showExtensions
                    mutationController: root.mutationController
                    clipboardController: root.clipboardController
                    onActivated: root.activateCurrent()
                    onContextMenuRequested: root.popupFor(root.selection.selectedEntries().length)
                }
            }
        }

        FileContextMenu {
            id: contextMenu
            objectName: "gridContextMenu"
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
}
