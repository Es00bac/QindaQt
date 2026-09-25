// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "EntryDrag.js" as EntryDrag

// Fixed places plus the user's persisted bookmarks. Activation only routes a
// path into NavigationController::navigateTo; a vanished bookmark therefore
// lands on the ordinary navigation state pane. Place and bookmark rows are
// drop targets for the identity-checked mutation pipeline; the Trash place is
// not, because a direct move into the Trash files directory would bypass the
// .trashinfo metadata the Trash contract requires.
Control {
    id: root
    objectName: "placesSidebar"

    required property var navigationController
    required property var placesController
    required property var appCoordinator
    required property var networkLocationsController
    // Optional drop dispatch targets; Main always passes the real controllers,
    // fixture tests may leave them null (drops then refuse politely).
    property var mutationController: null
    property var clipboardController: null
    // ADR-0271: the Explorer style's folder tree under the places, read
    // through ColumnListing.
    property bool showFolderTree: false
    property var columnListing: null

    implicitWidth: 196
    padding: 8

    background: Rectangle {
        color: root.palette.window
        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 1
            color: root.palette.mid
        }
    }

    function revealFocusedItem(item) {
        if (!item || sidebarScroll.height <= 0)
            return
        let ancestor = item
        while (ancestor && ancestor !== sidebarColumn)
            ancestor = ancestor.parent
        if (!ancestor)
            return
        const top = item.mapToItem(sidebarColumn, 0, 0).y
        const bottom = top + item.height
        const maximumY = Math.max(0, sidebarScroll.contentHeight - sidebarScroll.height)
        if (top < sidebarScroll.contentY)
            sidebarScroll.contentY = Math.max(0, Math.min(top, maximumY))
        else if (bottom > sidebarScroll.contentY + sidebarScroll.height)
            sidebarScroll.contentY = Math.max(0, Math.min(bottom - sidebarScroll.height, maximumY))
    }

    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() { root.revealFocusedItem(target.activeFocusItem) }
    }

    contentItem: Item {
        id: sidebarViewport

        Flickable {
            id: sidebarScroll
            objectName: "placesSidebarScroller"
            anchors.fill: parent
            anchors.rightMargin: 16
            contentWidth: width
            contentHeight: sidebarColumn.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: ViewportScrollBar {
                objectName: "placesSidebarScrollBar"
                parent: sidebarViewport
                x: sidebarScroll.width + 4
                height: sidebarScroll.height
                Accessible.name: qsTr("Scroll places, network locations and bookmarks")
            }

            // AGENT-GUARD: All sections belong to one scrollable column. A
            // bookmark-only viewport can be squeezed out by Places/Network.
            ColumnLayout {
                id: sidebarColumn
                width: sidebarScroll.width
                spacing: 4

                Label {
                    text: qsTr("Places")
                    color: root.palette.placeholderText
                    Accessible.ignored: true
                }

                Repeater {
                    model: root.placesController.places

                    PlaceButton {
                        required property var modelData
                        iconName: modelData.id === "home" ? "user-home"
                            : modelData.id === "trash" ? "user-trash"
                            : modelData.id === "applications" ? "folder-applications"
                            : modelData.id === "recents" ? "clock"
                            : modelData.id === "network" ? "network-workgroup" : "drive-harddisk"

                        // Places with no directory behind them open through their Go
                        // action and are never drop targets. Applications is browsed in
                        // the ordinary views (ADR-0262), so it is emphasized by its path;
                        // Network's empty path never matches.
                        readonly property bool routePlace: modelData.id === "network"
                            || modelData.id === "applications"

                        objectName: "placeButton_" + modelData.id
                        Layout.fillWidth: true
                        text: modelData.name
                        emphasized: modelData.path.length > 0
                            && root.navigationController.currentPath === modelData.path
                        // ADR-0194: the Network place opens the hub -- the saved
                        // locations and the way to add one -- rather than pretending a
                        // connection exists or dropping the user in the location bar.
                        // Applications opens the installed-application browser through
                        // the same action the Go menu uses, so there is one route.
                        Accessible.description: modelData.id === "network"
                            ? qsTr("Show saved network locations")
                            : modelData.id === "applications"
                            ? qsTr("Browse installed applications")
                            : modelData.id === "recents"
                            ? qsTr("Show the files you used most recently")
                            : qsTr("Open %1").arg(modelData.path)
                        onClicked: modelData.id === "network"
                            ? root.appCoordinator.activateAction("go.network")
                            : modelData.id === "applications"
                            ? root.appCoordinator.activateAction("go.applications")
                            : root.navigationController.navigateTo(modelData.path)

                        DropArea {
                            anchors.fill: parent
                            // ADR-0272: Recents is no folder to drop into.
                            enabled: modelData.id !== "trash" && modelData.id !== "recents"
                                     && !routePlace
                            onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                            onDropped: (drop) => {
                                const action = EntryDrag.dispatch(
                                    drop, modelData.path,
                                    root.mutationController, root.clipboardController)
                                if (action !== Qt.IgnoreAction)
                                    drop.accept(action)
                                else
                                    drop.accepted = false
                            }
                        }
                    }
                }

                Label {
                    Layout.topMargin: 8
                    visible: root.showFolderTree
                    text: qsTr("Folders")
                    color: root.palette.placeholderText
                    Accessible.ignored: true
                }

                Loader {
                    Layout.fillWidth: true
                    active: root.showFolderTree
                    visible: active
                    sourceComponent: FolderTree {
                        navigationController: root.navigationController
                        columnListing: root.columnListing
                        homePath: root.placesController.places.length > 0
                            ? root.placesController.places[0].path : ""
                    }
                }

                // Saved network locations the user asked to see here (ADR-0194).
                // Activation only routes the canonical address into navigateTo, so an
                // unreachable server lands on the ordinary navigation state pane.
                // They are not drop targets: the drop pipeline is the identity-checked
                // local mutation one, which has no network authority.
                Label {
                    Layout.topMargin: 8
                    visible: root.networkLocationsController.placesLocations.length > 0
                    text: qsTr("Network")
                    color: root.palette.placeholderText
                    Accessible.ignored: true
                }

                Repeater {
                    model: root.networkLocationsController.placesLocations

                    PlaceButton {
                        required property var modelData
                        iconName: "folder-network"
                        objectName: "networkPlaceButton_" + modelData.index
                        Layout.fillWidth: true
                        text: modelData.name
                        emphasized: root.navigationController.currentPath === modelData.url
                        Accessible.description: qsTr("Open %1").arg(modelData.url)
                        onClicked: root.navigationController.navigateTo(modelData.url)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Bookmarks")
                        color: root.palette.placeholderText
                        Accessible.ignored: true
                    }
                    IconButton {
                        iconName: "bookmark-new"
                        objectName: "addBookmarkButton"
                        text: qsTr("Bookmark this folder")
                        Accessible.description: qsTr("Bookmark the current folder")
                        onClicked: root.appCoordinator.activateAction("bookmark.add")
                    }
                }

                ColumnLayout {
                    objectName: "bookmarkList"
                    Layout.fillWidth: true
                    visible: root.placesController.bookmarks.length > 0
                    spacing: 0
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Bookmarks")

                    Repeater {
                        model: root.placesController.bookmarks
                        RowLayout {
                            id: bookmarkRow

                            required property var modelData

                            Layout.fillWidth: true
                            spacing: 4

                            PlaceButton {
                                iconName: "folder"
                                objectName: "bookmarkButton_" + bookmarkRow.modelData.index
                                Layout.fillWidth: true
                                text: bookmarkRow.modelData.name
                                emphasized: root.navigationController.currentPath === bookmarkRow.modelData.path
                                Accessible.description: qsTr("Open %1").arg(bookmarkRow.modelData.path)
                                onClicked: root.navigationController.navigateTo(bookmarkRow.modelData.path)

                                DropArea {
                                    anchors.fill: parent
                                    onEntered: (drag) => drag.accepted = EntryDrag.canAccept(drag)
                                    onDropped: (drop) => {
                                        const action = EntryDrag.dispatch(
                                            drop, bookmarkRow.modelData.path,
                                            root.mutationController, root.clipboardController)
                                        if (action !== Qt.IgnoreAction)
                                            drop.accept(action)
                                        else
                                            drop.accepted = false
                                    }
                                }
                            }
                            IconButton {
                                iconName: "edit-delete"
                                implicitWidth: 28
                                objectName: "removeBookmark_" + bookmarkRow.modelData.index
                                text: qsTr("Remove bookmark")
                                Accessible.description: qsTr("Remove bookmark %1").arg(bookmarkRow.modelData.name)
                                onClicked: root.placesController.removeBookmark(bookmarkRow.modelData.index)
                            }
                        }
                    }
                }
            }
        }
    }
}
