// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaTK as Tk

ToolBar {
    id: root
    objectName: "fileManagerToolbar"
    required property var navigationController
    required property var mutationController
    required property var appCoordinator
    property alias primaryFocusItem: newFolderButton
    property alias locationBar: locationBar
    signal browseRequested();
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
            onClosed: {
                visible = false
                root.browseRequested()
            }
        }
        IconButton {
            id: newFolderButton
            objectName: "newFolderButton"
            iconName: "folder-new"
            text: qsTr("New Folder")
            // ADR-0262/ADR-0272: nothing can be created inside the
            // Applications or Recents places.
            available: !root.navigationController.applicationsPlace
                       && !root.navigationController.recentsPlace
                       && ((!root.mutationController.busy && !root.navigationController.remoteActive)
                           || (root.navigationController.remoteCreateAvailable
                               && !root.navigationController.remoteCreateBusy))
            Accessible.description: qsTr("Create a folder in the current location")
            onClicked: root.appCoordinator.activateAction("file.new-folder")
        }
        // ADR-0270: the four views, in Finder's order. Each segment is named
        // by its text (a segment has no other accessible name), so the icons
        // join the words only where there is room, and below 600 px the
        // switcher gives way to the View menu and Ctrl+1 to Ctrl+4.
        Tk.Segmented {
            id: viewSwitcher
            objectName: "viewSwitcher"
            visible: root.width >= 600
            small: true
            tooltip: qsTr("View")
            readonly property var modes: ["grid", "list", "columns", "gallery"]
            readonly property var actions: ["view.grid-mode", "view.details-mode",
                                            "view.columns-mode", "view.gallery-mode"]
            readonly property bool roomy: root.width >= 820
            model: [
                { "text": qsTr("Icons"), "iconName": roomy ? "layout-grid" : "",
                  "value": "grid", "tooltip": qsTr("Icons (Ctrl+2)") },
                { "text": qsTr("Details"), "iconName": roomy ? "list" : "",
                  "value": "list", "tooltip": qsTr("Details (Ctrl+1)") },
                { "text": qsTr("Columns"), "iconName": roomy ? "columns-3" : "",
                  "value": "columns", "tooltip": qsTr("Columns (Ctrl+3)") },
                { "text": qsTr("Gallery"), "iconName": roomy ? "images" : "",
                  "value": "gallery", "tooltip": qsTr("Gallery (Ctrl+4)") }
            ]
            currentIndex: Math.max(0, modes.indexOf(root.navigationController.viewMode))
            // A click assigns currentIndex itself; the binding comes back so
            // the menu, the keys and a folder's own view keep it truthful.
            onActivated: (index) => {
                root.appCoordinator.activateAction(viewSwitcher.actions[index])
                viewSwitcher.currentIndex = Qt.binding(() =>
                    Math.max(0, viewSwitcher.modes.indexOf(root.navigationController.viewMode)))
            }
        }
        IconButton {
            objectName: "filterFolderButton"
            iconName: "edit-find"
            text: qsTr("Filter this folder (Ctrl+F)")
            available: !root.navigationController.remoteActive
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
                Repeater {
                    model: [{ "mode": "grid", "text": qsTr("Icon View"), "action": "view.grid-mode" },
                        { "mode": "list", "text": qsTr("Details View"), "action": "view.details-mode" },
                        { "mode": "columns", "text": qsTr("Columns View"), "action": "view.columns-mode" },
                        { "mode": "gallery", "text": qsTr("Gallery View"), "action": "view.gallery-mode" }]
                    MenuItem {
                        required property var modelData
                        text: modelData.text
                        checkable: true
                        autoExclusive: true
                        checked: root.navigationController.viewMode === modelData.mode
                        onTriggered: root.appCoordinator.activateAction(modelData.action)
                    }
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
