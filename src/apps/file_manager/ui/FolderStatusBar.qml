// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as Qinda
import QindaQt.Tokens 1.0

Rectangle {
    id: root
    required property var navigationController
    required property var selection
    required property var appCoordinator
    objectName: "folderStatusBar"
    color: Tokens.bg.raised
    implicitHeight: 40

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Tokens.space["3"]
        anchors.rightMargin: Tokens.space["2"]
        spacing: Tokens.space["1"]
        Qinda.Label {
            objectName: "folderSelectionCount"
            Layout.fillWidth: true
            muted: true
            text: root.selection.count() > 0
                ? qsTr("%1 selected").arg(root.selection.count())
                : qsTr("%1 items").arg(root.navigationController.entries.length)
            elide: Text.ElideRight
        }
        IconButton {
            objectName: "zoomOutButton"
            iconName: "list-remove"
            text: qsTr("Zoom out (Ctrl+−)")
            available: root.navigationController.canZoomOut
            implicitWidth: 32
            implicitHeight: 32
            onClicked: root.appCoordinator.activateAction("view.zoom-out")
        }
        Qinda.Button {
            objectName: "resetZoomButton"
            text: Math.round(root.navigationController.iconSize / 64 * 100) + "%"
            emphasized: false
            accessibleDescription: qsTr("Reset icon size (Ctrl+0)")
            onClicked: root.appCoordinator.activateAction("view.zoom-reset")
        }
        IconButton {
            objectName: "zoomInButton"
            iconName: "list-add"
            text: qsTr("Zoom in (Ctrl++)")
            available: root.navigationController.canZoomIn
            implicitWidth: 32
            implicitHeight: 32
            onClicked: root.appCoordinator.activateAction("view.zoom-in")
        }
    }
    Rectangle { width: parent.width; height: 1; color: Tokens.outline.divider }
}
