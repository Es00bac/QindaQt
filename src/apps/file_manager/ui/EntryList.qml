// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "EntryDrag.js" as EntryDrag

Control {
    id: root

    required property var navigationController
    required property var selection
    required property var appCoordinator
    // Optional drop dispatch targets; Main always passes the real controllers,
    // fixture tests may leave them null (drops then refuse politely).
    property var mutationController: null
    property var clipboardController: null

    property int iconSize: 64
    readonly property int rowIconSize: Math.max(20, Math.round(iconSize * 0.4375))
    readonly property int rowHeight: rowIconSize + 16
    signal zoomRequested(int steps)

    ViewportNavigation {
        id: keyboardNavigation
        view: listView
        selection: root.selection
        navigationController: root.navigationController
        columns: 1
        rowHeight: root.rowHeight
        onContextMenuRequested: contextMenu.popup()
    }
    onIconSizeChanged: keyboardNavigation.scheduleReveal()

    readonly property alias focusItem: listView

    function focusView() { listView.forceActiveFocus() }

    function currentEntry() {
        return listView.currentIndex >= 0
            ? root.navigationController.entries[listView.currentIndex] : null
    }

    function selectedEntries() {
        return selection.selectedEntries()
    }

    function selectAll() {
        selection.selectAll()
    }

    function activateCurrent() {
        if (listView.currentIndex >= 0) {
            root.navigationController.activate(listView.currentIndex)
        }
    }

    // Listing notices remain outside the scrolling entries.
    padding: 8

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            id: headerRow
            Layout.fillWidth: true
            Layout.rightMargin: 16
            spacing: 0

            // AGENT-NOTE: The sort headers are statically declared buttons, not
            // a Repeater, because model-instantiated delegates are not
            // reachable through QObject::findChild from the QML root; the
            // --check-ui-contract gate and the UI action probe resolve
            // sortHeader_* by objectName. The column set is fixed by
            // ListingOrder (model/listing_order.h).
            component SortHeaderButton: Button {
                required property string key
                required property string label
                property bool stretch: false

                Layout.fillWidth: stretch
                Layout.preferredWidth: stretch ? -1 : 112
                flat: root.navigationController.sortColumn !== key
                text: label + (root.navigationController.sortColumn === key
                    ? (root.navigationController.sortDirection === "ascending" ? " ▲" : " ▼") : "")
                Accessible.description: qsTr("Sort by %1").arg(label)
                onClicked: root.navigationController.setSortColumn(key)
            }

            SortHeaderButton {
                objectName: "sortHeader_name"
                key: "name"
                label: qsTr("Name")
                stretch: true
            }
            SortHeaderButton {
                objectName: "sortHeader_size"
                key: "size"
                label: qsTr("Size")
            }
            SortHeaderButton {
                objectName: "sortHeader_kind"
                visible: root.width > 580
                key: "kind"
                label: qsTr("Kind")
            }
            SortHeaderButton {
                objectName: "sortHeader_modified"
                visible: root.width > 440
                key: "modified"
                label: qsTr("Modified")
            }
        }

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

            ListView {
                id: listView
                objectName: "entryListView"
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
                model: root.navigationController.entries

                ScrollBar.vertical: ViewportScrollBar {
                    objectName: "entryListScrollBar"
                    parent: viewport
                    x: listView.width + 4
                    height: listView.height
                }
                ZoomWheelHandler { onZoomRequested: (steps) => root.zoomRequested(steps) }

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Folder contents")

                Keys.onReturnPressed: root.activateCurrent()
                Keys.onEnterPressed: root.activateCurrent()
                Keys.onPressed: (event) => keyboardNavigation.handle(event)

                delegate: EntryListDelegate {
                    selection: root.selection
                    viewPalette: root.palette
                    rowHeight: root.rowHeight
                    rowIconSize: root.rowIconSize
                    viewWidth: root.width
                    mutationController: root.mutationController
                    clipboardController: root.clipboardController
                    onActivated: { root.activateCurrent(); }
                    onContextMenuRequested: { contextMenu.popup(); }
                }
            }

        }

        Menu {
            id: contextMenu
            objectName: "entryContextMenu"

            MenuItem {
                objectName: "contextCutAction"
                text: qsTr("Cut")
                onTriggered: root.appCoordinator.activateAction("edit.cut")
            }
            MenuItem {
                objectName: "contextClipboardCopyAction"
                text: qsTr("Copy")
                onTriggered: root.appCoordinator.activateAction("edit.copy")
            }
            MenuItem {
                objectName: "contextPasteAction"
                text: qsTr("Paste")
                onTriggered: root.appCoordinator.activateAction("edit.paste")
            }
            MenuItem {
                objectName: "contextRenameAction"
                text: qsTr("Rename")
                onTriggered: root.appCoordinator.activateAction("file.rename")
            }
            MenuItem {
                objectName: "contextCopyAction"
                text: qsTr("Copy To…")
                onTriggered: root.appCoordinator.activateAction("file.copy")
            }
            MenuItem {
                objectName: "contextMoveAction"
                text: qsTr("Move To…")
                onTriggered: root.appCoordinator.activateAction("file.move")
            }
            MenuItem {
                objectName: "contextTrashAction"
                text: qsTr("Move to Trash")
                onTriggered: root.appCoordinator.activateAction("file.trash")
            }
            MenuItem {
                objectName: "contextPropertiesAction"
                text: qsTr("Properties")
                onTriggered: root.appCoordinator.activateAction("file.properties")
            }
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
