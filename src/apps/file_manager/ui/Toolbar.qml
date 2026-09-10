// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: root
    objectName: "fileManagerToolbar"
    required property var navigationController
    required property var mutationController
    required property var appCoordinator
    property alias primaryFocusItem: newFolderButton
    property alias locationBar: locationBar
    padding: 4

    RowLayout {
        anchors.fill: parent
        spacing: 4
        IconButton {
            objectName: "navigateBackButton"
            iconName: "go-previous"
            text: qsTr("Back")
            available: root.navigationController.canGoBack
            Accessible.description: qsTr("Return to the previous folder")
            onClicked: root.appCoordinator.activateAction("go.back")
        }
        IconButton {
            objectName: "navigateForwardButton"
            iconName: "go-next"
            text: qsTr("Forward")
            available: root.navigationController.canGoForward
            Accessible.description: qsTr("Return to the folder undone by Back")
            onClicked: root.appCoordinator.activateAction("go.forward")
        }
        IconButton {
            objectName: "navigateUpButton"
            iconName: "go-up"
            text: qsTr("Up")
            available: root.navigationController.canGoUp
            Accessible.description: qsTr("Open the parent folder")
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
            Accessible.description: qsTr("Create a folder in the current location")
            onClicked: root.appCoordinator.activateAction("file.new-folder")
        }
        IconButton {
            objectName: "toggleViewModeButton"
            iconName: root.navigationController.viewMode === "grid" ? "view-list-details" : "view-grid"
            text: root.navigationController.viewMode === "grid" ? qsTr("Details View") : qsTr("Icon View")
            Accessible.description: qsTr("Switch between the detailed list and the icon grid")
            onClicked: root.appCoordinator.activateAction(root.navigationController.viewMode === "grid"
                ? "view.details-mode" : "view.grid-mode")
        }
        IconButton {
            objectName: "filterFolderButton"
            iconName: "edit-find"
            text: qsTr("Filter this folder (Ctrl+F)")
            Accessible.description: qsTr("Filter or search below this folder by name")
            onClicked: root.appCoordinator.activateAction("view.filter")
        }
        IconButton {
            iconName: "application-menu"
            text: qsTr("Folder options")
            Accessible.description: qsTr("Open the folder options menu")
            onClicked: options.popup()
            Menu {
                id: options
                MenuItem {
                    objectName: "locationToggleButton"
                    text: qsTr("Enter Location…")
                    onTriggered: root.appCoordinator.activateAction("view.focus-location")
                }
                MenuItem {
                    objectName: "toggleHiddenButton"
                    text: qsTr("Show Hidden Files")
                    checkable: true
                    checked: root.navigationController.showHidden
                    onTriggered: root.appCoordinator.activateAction("view.show-hidden")
                }
                MenuItem {
                    objectName: "refreshButton"
                    text: qsTr("Refresh")
                    onTriggered: root.appCoordinator.activateAction("view.refresh")
                }
                MenuSeparator {}
                MenuItem {
                    text: qsTr("Details View")
                    checkable: true
                    autoExclusive: true
                    checked: root.navigationController.viewMode === "list"
                    onTriggered: root.appCoordinator.activateAction("view.details-mode")
                }
                MenuItem {
                    text: qsTr("Icon View")
                    checkable: true
                    autoExclusive: true
                    checked: root.navigationController.viewMode === "grid"
                    onTriggered: root.appCoordinator.activateAction("view.grid-mode")
                }
                MenuItem {
                    text: qsTr("Zoom In")
                    enabled: root.navigationController.canZoomIn
                    onTriggered: root.appCoordinator.activateAction("view.zoom-in")
                }
                MenuItem {
                    text: qsTr("Zoom Out")
                    enabled: root.navigationController.canZoomOut
                    onTriggered: root.appCoordinator.activateAction("view.zoom-out")
                }
                MenuItem {
                    text: qsTr("Reset Zoom")
                    onTriggered: root.appCoordinator.activateAction("view.zoom-reset")
                }
                MenuSeparator {}
                Menu {
                    title: qsTr("Sort By")
                    Repeater {
                        model: [{key: "name", label: qsTr("Name")}, {key: "size", label: qsTr("Size")},
                            {key: "kind", label: qsTr("Kind")}, {key: "modified", label: qsTr("Modified")}]
                        MenuItem {
                            required property var modelData
                            text: modelData.label
                            checkable: true
                            checked: root.navigationController.sortColumn === modelData.key
                            onTriggered: root.navigationController.setSortColumn(modelData.key)
                        }
                    }
                }
                MenuItem {
                    objectName: "restoreLastButton"
                    text: qsTr("Restore Last Trashed Item")
                    enabled: root.mutationController.canRestore
                    onTriggered: root.appCoordinator.activateAction("file.restore-last")
                }
            }
        }
    }
}
