// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

// Fixed places plus the user's persisted bookmarks. Activation only routes a
// path into NavigationController::navigateTo; a vanished bookmark therefore
// lands on the ordinary navigation state card.
Rectangle {
    id: root
    objectName: "placesSidebar"

    required property var navigationController
    required property var placesController
    required property var appCoordinator

    implicitWidth: 220
    color: Tokens.bg.base

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: Tokens.space["1"]

        Qinda.Label {
            text: qsTr("Places")
            muted: true
            Accessible.ignored: true
        }

        Repeater {
            model: root.placesController.places

            Qinda.Button {
                required property var modelData

                objectName: "placeButton_" + modelData.id
                Layout.fillWidth: true
                text: modelData.name
                emphasized: root.navigationController.currentPath === modelData.path
                accessibleDescription: qsTr("Open %1").arg(modelData.path)
                onClicked: root.navigationController.navigateTo(modelData.path)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Tokens.space["2"]

            Qinda.Label {
                Layout.fillWidth: true
                text: qsTr("Bookmarks")
                muted: true
                Accessible.ignored: true
            }
            Qinda.Button {
                objectName: "addBookmarkButton"
                text: qsTr("Add")
                emphasized: false
                accessibleDescription: qsTr("Bookmark the current folder")
                onClicked: root.appCoordinator.activateAction("bookmark.add")
            }
        }

        ListView {
            id: bookmarkList
            objectName: "bookmarkList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.placesController.bookmarks

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Bookmarks")

            delegate: RowLayout {
                id: bookmarkRow

                required property var modelData

                width: bookmarkList.width
                spacing: Tokens.space["1"]

                Qinda.Button {
                    objectName: "bookmarkButton_" + bookmarkRow.modelData.index
                    Layout.fillWidth: true
                    text: bookmarkRow.modelData.name
                    emphasized: root.navigationController.currentPath === bookmarkRow.modelData.path
                    accessibleDescription: qsTr("Open %1").arg(bookmarkRow.modelData.path)
                    onClicked: root.navigationController.navigateTo(bookmarkRow.modelData.path)
                }
                Qinda.Button {
                    objectName: "removeBookmark_" + bookmarkRow.modelData.index
                    text: qsTr("Remove")
                    emphasized: false
                    accessibleDescription: qsTr("Remove bookmark %1").arg(bookmarkRow.modelData.name)
                    onClicked: root.placesController.removeBookmark(bookmarkRow.modelData.index)
                }
            }
        }

        Qinda.Label {
            Layout.fillWidth: true
            visible: root.placesController.bookmarks.length === 0
            text: qsTr("No bookmarks yet")
            muted: true
            Accessible.ignored: true
        }
    }
}
