// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Qinda.MaterialSurface {
    id: root
    radius: 0
    required property var navigationController
    required property var mutationController
    required property var appCoordinator
    property alias primaryFocusItem: newFolderButton
    property alias locationBar: locationBar
    implicitHeight: 60
    color: Tokens.bg.raised

    RowLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: Tokens.space["1"]
        IconButton {
            objectName: "navigateBackButton"
            iconName: "go-previous"
            text: qsTr("Back")
            available: root.navigationController.canGoBack
            onClicked: root.appCoordinator.activateAction("go.back")
        }
        IconButton {
            objectName: "navigateForwardButton"
            iconName: "go-next"
            text: qsTr("Forward")
            visible: root.width >= 680
            available: root.navigationController.canGoForward
            onClicked: root.appCoordinator.activateAction("go.forward")
        }
        IconButton {
            objectName: "navigateUpButton"
            iconName: "go-up"
            text: qsTr("Up")
            available: root.navigationController.canGoUp
            onClicked: root.appCoordinator.activateAction("go.up")
        }
        Breadcrumb {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 60
            visible: !locationBar.visible
            navigationController: root.navigationController
        }
        LocationBar {
            id: locationBar
            Layout.fillWidth: true
            Layout.minimumWidth: 60
            visible: false
            navigationController: root.navigationController
            onClosed: visible = false
        }
        IconButton {
            id: newFolderButton
            objectName: "newFolderButton"
            iconName: "folder-new"
            text: qsTr("New Folder")
            available: !root.mutationController.busy
            onClicked: root.appCoordinator.activateAction("file.new-folder")
        }
        IconButton {
            objectName: "toggleViewModeButton"
            iconName: root.navigationController.viewMode === "grid" ? "view-list-details" : "view-grid"
            text: root.navigationController.viewMode === "grid" ? qsTr("Details View") : qsTr("Icon View")
            onClicked: root.appCoordinator.activateAction("view.grid-mode")
        }
        IconButton {
            iconName: "application-menu"
            text: qsTr("Folder options")
            onClicked: options.popup()
            T.Menu {
                id: options
                T.MenuItem {
                    objectName: "locationToggleButton"
                    text: qsTr("Enter Location…")
                    onTriggered: root.appCoordinator.activateAction("view.focus-location")
                }
                T.MenuItem {
                    objectName: "toggleHiddenButton"
                    text: qsTr("Show Hidden Files")
                    checkable: true
                    checked: root.navigationController.showHidden
                    onTriggered: root.appCoordinator.activateAction("view.show-hidden")
                }
                T.MenuItem {
                    objectName: "refreshButton"
                    text: qsTr("Refresh")
                    onTriggered: root.appCoordinator.activateAction("view.refresh")
                }
                T.MenuSeparator {}
                T.Menu {
                    title: qsTr("Sort By")
                    Repeater {
                        model: [{key: "name", label: qsTr("Name")}, {key: "size", label: qsTr("Size")},
                            {key: "kind", label: qsTr("Kind")}, {key: "modified", label: qsTr("Modified")}]
                        T.MenuItem {
                            required property var modelData
                            text: modelData.label
                            checkable: true
                            checked: root.navigationController.sortColumn === modelData.key
                            onTriggered: root.navigationController.setSortColumn(modelData.key)
                        }
                    }
                }
                T.MenuItem {
                    objectName: "restoreLastButton"
                    text: qsTr("Restore Last Trashed Item")
                    enabled: root.mutationController.canRestore
                    onTriggered: root.appCoordinator.activateAction("file.restore-last")
                }
            }
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Tokens.outline.divider }
}
