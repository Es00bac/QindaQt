// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Rectangle {
    id: root

    required property var navigationController
    required property var mutationController
    required property var appCoordinator
    property alias primaryFocusItem: newFolderButton

    implicitHeight: row.implicitHeight + Tokens.space["3"] * 2
    color: Tokens.bg.raised

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Tokens.space["3"]
        spacing: Tokens.space["2"]

        Qinda.Button {
            id: newFolderButton
            objectName: "newFolderButton"
            text: qsTr("New Folder")
            available: !root.mutationController.busy
            emphasized: true
            accessibleDescription: qsTr("Create a folder in the current location")
            onClicked: root.appCoordinator.activateAction("file.new-folder")
        }
        Qinda.Button {
            objectName: "navigateBackButton"
            text: qsTr("Back")
            available: root.navigationController.canGoBack
            emphasized: false
            accessibleDescription: qsTr("Go to the previous folder")
            onClicked: root.navigationController.goBack()
        }
        Qinda.Button {
            objectName: "navigateForwardButton"
            text: qsTr("Forward")
            available: root.navigationController.canGoForward
            emphasized: false
            accessibleDescription: qsTr("Go to the next folder")
            onClicked: root.navigationController.goForward()
        }
        Qinda.Button {
            objectName: "navigateUpButton"
            text: qsTr("Up")
            available: root.navigationController.canGoUp
            emphasized: false
            accessibleDescription: qsTr("Go to the parent folder")
            onClicked: root.navigationController.goUp()
        }
        Item { Layout.fillWidth: true }
        Qinda.Button {
            objectName: "restoreLastButton"
            text: qsTr("Restore")
            available: root.mutationController.canRestore
            emphasized: false
            accessibleDescription: qsTr("Restore the last item moved to Trash")
            onClicked: root.appCoordinator.activateAction("file.restore-last")
        }
        Qinda.Button {
            objectName: "refreshButton"
            text: qsTr("Refresh")
            emphasized: false
            accessibleDescription: qsTr("Reload the current folder")
            onClicked: root.navigationController.refresh()
        }
    }
}
