// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root
    required property var navigationController
    required property var selection
    required property var appCoordinator
    objectName: "folderStatusBar"
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 8

    background: Rectangle {
        color: root.palette.window
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: root.palette.mid }
    }

    contentItem: RowLayout {
        spacing: 4
        Label {
            objectName: "folderSelectionCount"
            Layout.fillWidth: true
            color: root.palette.placeholderText
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
        Button {
            objectName: "resetZoomButton"
            flat: true
            text: Math.round(root.navigationController.iconSize / 64 * 100) + "%"
            Accessible.description: qsTr("Reset icon size (Ctrl+0)")
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
}
