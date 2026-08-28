// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Rectangle {
    id: root

    required property var navigationController

    implicitHeight: row.implicitHeight + Tokens.space["3"] * 2
    color: Tokens.bg.raised

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Tokens.space["3"]
        spacing: Tokens.space["2"]

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
            objectName: "refreshButton"
            text: qsTr("Refresh")
            emphasized: false
            accessibleDescription: qsTr("Reload the current folder")
            onClicked: root.navigationController.refresh()
        }
    }
}
