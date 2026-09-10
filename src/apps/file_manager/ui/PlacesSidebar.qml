// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Fixed places plus the user's persisted bookmarks. Activation only routes a
// path into NavigationController::navigateTo; a vanished bookmark therefore
// lands on the ordinary navigation state pane.
Control {
    id: root
    objectName: "placesSidebar"

    required property var navigationController
    required property var placesController
    required property var appCoordinator

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

    contentItem: ColumnLayout {
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
                iconName: modelData.id === "home" ? "user-home" : modelData.id === "trash" ? "user-trash" : "drive-harddisk"

                objectName: "placeButton_" + modelData.id
                Layout.fillWidth: true
                text: modelData.name
                emphasized: root.navigationController.currentPath === modelData.path
                Accessible.description: qsTr("Open %1").arg(modelData.path)
                onClicked: root.navigationController.navigateTo(modelData.path)
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

        Item {
            id: bookmarkViewport
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: bookmarkList
                objectName: "bookmarkList"
                anchors.fill: parent
                anchors.rightMargin: 16
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                model: root.placesController.bookmarks

                ScrollBar.vertical: ViewportScrollBar {
                    objectName: "bookmarkScrollBar"
                    parent: bookmarkViewport
                    x: bookmarkList.width + 4
                    height: bookmarkList.height
                    Accessible.name: qsTr("Scroll bookmarks")
                }

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Bookmarks")

                delegate: RowLayout {
                    id: bookmarkRow

                    required property var modelData

                    width: bookmarkList.width
                    spacing: 4

                    PlaceButton {
                        iconName: "folder"
                        objectName: "bookmarkButton_" + bookmarkRow.modelData.index
                        Layout.fillWidth: true
                        text: bookmarkRow.modelData.name
                        emphasized: root.navigationController.currentPath === bookmarkRow.modelData.path
                        Accessible.description: qsTr("Open %1").arg(bookmarkRow.modelData.path)
                        onClicked: root.navigationController.navigateTo(bookmarkRow.modelData.path)
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
