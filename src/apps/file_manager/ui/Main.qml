// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ADR-0116: stock Qt Quick Controls only — no QindaQt.Tokens/Controls imports
// and no palette literals; appearance comes from the Qt platform theme
// (ADR-0115) with QT_QUICK_CONTROLS_STYLE=Fusion set by the session.
// The AppShell seams stay non-visual: the window binds the injected
// ApplicationCoordinator for the action catalog, in-window menus, quit
// arbitration, focus reporting, and the degraded-integration notice.
ApplicationWindow {
    id: root

    required property var coordinator
    required property var navigationController
    required property var mutationController
    required property var clipboardController
    required property var propertiesController
    required property var searchController
    required property var placesController

    property bool closeAuthorized: false
    property bool inWindowMenuVisible: true

    visible: true
    width: 900
    height: 600
    minimumWidth: 480
    minimumHeight: 320
    title: coordinator.windowTitle.length > 0
           ? coordinator.windowTitle : coordinator.applicationName

    // AGENT-CONTRACT: Closing asks the owning application for a decision. The
    // coordinator and this surface never call QCoreApplication::quit or infer
    // whether domain state (a running file operation) is safe to abandon.
    onClosing: function(close) {
        if (closeAuthorized) {
            close.accepted = true
            return
        }
        close.accepted = false
        coordinator.requestQuit("window-close")
    }

    onActiveFocusItemChanged: {
        const owner = activeFocusItem && activeFocusItem.objectName
                    ? activeFocusItem.objectName : ""
        coordinator.reportFocusOwner(owner)
    }

    Component.onCompleted: {
        if (coordinator.initialFocusObjectName === toolbar.primaryFocusItem.objectName)
            toolbar.primaryFocusItem.forceActiveFocus(Qt.TabFocusReason)
        else
            root.activeView().focusView()
    }

    Connections {
        target: root.coordinator
        function onQuitApproved(requestId) {
            root.closeAuthorized = true
            root.close()
        }
    }

    Shortcut { sequence: "Ctrl+="; onActivated: root.coordinator.activateAction("view.zoom-in") }
    Shortcut { sequence: "Ctrl+R"; onActivated: root.coordinator.activateAction("view.refresh") }

    EntrySelection {
        id: entrySelection
        objectName: "entrySelection"
        navigationController: root.navigationController
        onSelectedChanged: root.clipboardController.selectionCount = count()
    }

    function activeView() {
        return root.navigationController.viewMode === "grid" ? entryGrid : entryList
    }

    // Paste lands inside the focused folder entry when one exists, otherwise
    // in the folder being browsed.
    function pasteDestination() {
        const entry = root.activeView().currentEntry()
        if (entry && entry.isDirectory)
            return entry.path
        return root.navigationController.currentPath
    }

    Connections {
        target: root.coordinator
        function onActionRequested(actionId) {
            const navigation = root.navigationController
            if (actionId === "go.back") {
                navigation.goBack()
            } else if (actionId === "go.forward") {
                navigation.goForward()
            } else if (actionId === "go.up") {
                navigation.goUp()
            } else if (actionId === "view.refresh") {
                navigation.refresh()
            } else if (actionId === "view.show-hidden") {
                navigation.setShowHidden(!navigation.showHidden)
            } else if (actionId === "view.grid-mode") {
                navigation.setViewMode("grid")
            } else if (actionId === "view.details-mode") {
                navigation.setViewMode("list")
            } else if (actionId === "view.zoom-in") {
                navigation.zoomBy(1)
            } else if (actionId === "view.zoom-out") {
                navigation.zoomBy(-1)
            } else if (actionId === "view.zoom-reset") {
                navigation.resetZoom()
            } else if (actionId === "view.filter") {
                filterBar.visible = true
                filterBar.activate()
            } else if (actionId === "view.focus-location") {
                toolbar.locationBar.visible = true
                toolbar.locationBar.activate()
            } else if (actionId === "edit.select-all") {
                root.activeView().selectAll()
            } else if (actionId === "edit.cut") {
                root.clipboardController.cutSelection(root.activeView().selectedEntries())
            } else if (actionId === "edit.copy") {
                root.clipboardController.copySelection(root.activeView().selectedEntries())
            } else if (actionId === "edit.paste") {
                root.clipboardController.pasteInto(root.pasteDestination())
            } else if (actionId === "file.properties") {
                root.propertiesController.inspect(root.activeView().selectedEntries())
                if (root.propertiesController.active)
                    propertiesDialog.open()
            } else if (actionId === "go.home") {
                const places = root.placesController.places
                if (places.length > 0)
                    navigation.navigateTo(places[0].path)
            } else if (actionId === "bookmark.add") {
                root.placesController.addBookmark(
                    navigation.breadcrumb.length > 0
                        ? navigation.breadcrumb[navigation.breadcrumb.length - 1].name
                        : navigation.currentPath,
                    navigation.currentPath)
            } else {
                mutationDialogs.dispatch(actionId, root.activeView().selectedEntries())
            }
        }
    }

    readonly property string displayedViewMode: root.navigationController.viewMode
    onDisplayedViewModeChanged: Qt.callLater(() => root.activeView().focusView())

    Connections {
        target: root.mutationController
        function onMutationCommitted() {
            // While search results are on screen, re-run the bounded search
            // instead of refreshing: a plain refresh would silently drop the
            // guest listing the user is acting on.
            if (root.navigationController.guestListingActive)
                root.searchController.restart()
            else
                root.navigationController.refresh()
        }
    }

    // In-window menu authority, hidden when the global-menu export claims the
    // window (composeFileManagerMenuExport flips inWindowMenuVisible).
    menuBar: MenuBar {
        id: exportedMenuBar
        objectName: "appShellMenuBar"
        visible: root.inWindowMenuVisible

        Instantiator {
            model: root.coordinator.menus

            delegate: Menu {
                id: exportedMenu
                required property var modelData
                title: modelData.label

                Instantiator {
                    model: exportedMenu.modelData.actions

                    delegate: Action {
                        required property var modelData
                        text: modelData.label
                        enabled: modelData.enabled
                        checkable: modelData.checkable
                        checked: modelData.checked
                        shortcut: modelData.shortcut
                        onTriggered: root.coordinator.activateAction(modelData.id)
                    }

                    onObjectAdded: function(index, object) {
                        exportedMenu.insertAction(index, object)
                    }
                    onObjectRemoved: function(index, object) {
                        exportedMenu.removeAction(object)
                    }
                }
            }

            onObjectAdded: function(index, object) {
                exportedMenuBar.insertMenu(index, object)
            }
            onObjectRemoved: function(index, object) {
                exportedMenuBar.removeMenu(object)
            }
        }
    }

    // Degraded AppShell integrations remain usable; keep the notice's title
    // distinct for an unavailable integration.
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StatusBanner {
            objectName: "appShellDegradedNotice"
            Layout.fillWidth: true
            visible: root.coordinator.degraded
            title: root.coordinator.hasUnavailableIntegration
                ? qsTr("Feature unavailable") : qsTr("Limited capability")
            message: root.coordinator.degradedMessage
        }

        Toolbar {
            id: toolbar
            Layout.fillWidth: true
            navigationController: root.navigationController
            mutationController: root.mutationController
            appCoordinator: root.coordinator
        }

        FilterBar {
            id: filterBar
            Layout.fillWidth: true
            visible: false
            navigationController: root.navigationController
            searchController: root.searchController
            onClosed: {
                root.searchController.cancel()
                if (root.navigationController.guestListingActive)
                    root.navigationController.clearGuestListing()
                root.navigationController.setNameFilter("")
                visible = false
                root.activeView().focusView()
            }
            onBrowseRequested: root.activeView().focusView()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            PlacesSidebar {
                Layout.preferredWidth: root.width < 680 ? 148 : 196
                Layout.fillHeight: true
                navigationController: root.navigationController
                placesController: root.placesController
                appCoordinator: root.coordinator
                mutationController: root.mutationController
                clipboardController: root.clipboardController
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.navigationController.statusKey === "ready" ? 0 : 1

                StackLayout {
                    currentIndex: root.navigationController.viewMode === "grid" ? 1 : 0

                    EntryList {
                        id: entryList
                        iconSize: root.navigationController.iconSize
                        onZoomRequested: (steps) => root.navigationController.zoomBy(steps)
                        selection: entrySelection
                        navigationController: root.navigationController
                        appCoordinator: root.coordinator
                        mutationController: root.mutationController
                        clipboardController: root.clipboardController
                    }

                    EntryGrid {
                        id: entryGrid
                        iconSize: root.navigationController.iconSize
                        onZoomRequested: (steps) => root.navigationController.zoomBy(steps)
                        selection: entrySelection
                        navigationController: root.navigationController
                        appCoordinator: root.coordinator
                        mutationController: root.mutationController
                        clipboardController: root.clipboardController
                    }
                }

                StatePane {
                    statusKey: root.navigationController.statusKey
                    statusMessage: root.navigationController.statusMessage
                    onRetryRequested: root.navigationController.refresh()
                }
            }
        }

        FolderStatusBar {
            Layout.fillWidth: true
            navigationController: root.navigationController
            selection: entrySelection
            appCoordinator: root.coordinator
        }

        StatusBanner {
            objectName: "mutationProgressCard"
            Layout.fillWidth: true
            visible: root.mutationController.busy
            title: qsTr("File operation in progress")
            message: qsTr("%1. Progress %2 percent")
                .arg(root.mutationController.progressText)
                .arg(root.mutationController.progressValue)
            actionText: qsTr("Cancel")
            onActionTriggered: root.mutationController.cancel()
        }

        StatusBanner {
            objectName: "mutationFailureCard"
            Layout.fillWidth: true
            visible: root.mutationController.failureCode !== "none"
            title: qsTr("File operation failed: %1")
                .arg(root.mutationController.failureCode)
            message: root.mutationController.failureMessage
            actionText: qsTr("Dismiss")
            onActionTriggered: root.mutationController.clearFailure()
        }

        StatusBanner {
            objectName: "mutationResultCard"
            Layout.fillWidth: true
            visible: !root.mutationController.busy
                && root.mutationController.failureCode === "none"
                && root.mutationController.resultText.length > 0
            title: root.mutationController.resultText
            message: root.mutationController.canRestore
                ? qsTr("The most recently trashed item can be restored.") : ""
            actionText: root.mutationController.canUndo ? qsTr("Undo") : ""
            onActionTriggered: root.mutationController.undo()
        }

        StatusBanner {
            objectName: "launchErrorBanner"
            Layout.fillWidth: true
            visible: root.navigationController.launchError.length > 0
            title: qsTr("Couldn't open the file")
            message: root.navigationController.launchError
            actionText: qsTr("Dismiss")
            onActionTriggered: root.navigationController.clearLaunchError()
        }

        StatusBanner {
            objectName: "bookmarkStoreBanner"
            Layout.fillWidth: true
            visible: root.placesController.storeError.length > 0
            title: qsTr("Bookmark storage problem")
            message: root.placesController.storeError
            actionText: qsTr("Dismiss")
            onActionTriggered: root.placesController.clearStoreError()
        }
    }

    // Extra mouse buttons share the same history actions as toolbar/menu.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        onClicked: (event) => root.coordinator.activateAction(
            event.button === Qt.BackButton ? "go.back" : "go.forward")
    }

    MutationDialogs {
        id: mutationDialogs
        anchors.fill: parent
        navigationController: root.navigationController
        mutationController: root.mutationController
    }

    PropertiesDialog {
        id: propertiesDialog
        objectName: "propertiesDialog"
        controller: root.propertiesController
    }
}
