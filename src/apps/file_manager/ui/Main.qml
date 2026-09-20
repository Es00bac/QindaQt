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
    required property var applicationsController
    required property var networkLocationsController
    required property var transferQueueController
    required property var preferencesController
    required property var discoveryController
    required property var mountManager

    property bool closeAuthorized: false
    property bool inWindowMenuVisible: true
    // The Applications browser (ADR-0164) replaces the folder views while
    // true. Browsing to any folder path exits it; "go.applications" enters.
    property bool applicationsMode: false
    // ADR-0194: the Network place's hub replaces the folder views while true.
    // Browsing to any folder leaves it, exactly as the Applications browser does.
    property bool networkMode: false
    Binding {
        target: root.navigationController
        property: "folderViewActive"
        value: !root.applicationsMode && !root.networkMode
    }
    // ADR-0165: --choose-application turns the Applications browser into a
    // workspace picker; a successful choice quits the picker window after
    // the compositor closes it (see chooserSucceeded handling below).
    property bool chooserMode: false

    // Set by FileContextMenu immediately before it activates "edit.paste" for
    // a background (empty-space) invocation, and cleared by it immediately
    // after: pasteDestination()'s focused-entry lookup below is wrong for a
    // background paste (an unrelated directory can still be focused), but the
    // action must still cross the coordinator like every other trigger, so
    // this is the narrow, self-clearing seam that lets the one background
    // case override the destination without adding a second dispatch path.
    property string backgroundPasteOverride: ""

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

    Connections {
        target: root.applicationsController
        // The compositor closes the picker when the chosen application's
        // window replaces it, so the picker itself only needs to quit cleanly
        // through the standard arbitration once the choice is accepted.
        function onChooserSucceeded() {
            root.coordinator.requestQuit("application-chosen")
        }
    }

    Connections {
        target: root.navigationController
        // Browsing to any folder exits the Applications browser; the folder
        // views are the default surface and a Places click must land there.
        function onNavigationChanged() {
            root.applicationsMode = false
            root.networkMode = false
        }
    }

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
    // in the folder being browsed. A background invocation overrides that
    // lookup (see backgroundPasteOverride above) since it must always target
    // the browsed folder regardless of what else remains focused.
    function pasteDestination() {
        if (root.backgroundPasteOverride.length > 0)
            return root.backgroundPasteOverride
        const entry = root.activeView().currentEntry()
        if (entry && entry.isDirectory)
            return entry.path
        return root.navigationController.currentPath
    }

    Connections {
        target: root.coordinator
        function onActionRequested(actionId) {
            const navigation = root.navigationController
            if (actionId === "go.applications") {
                if (filterBar.visible) filterBar.closed()
                root.networkMode = false
                root.applicationsMode = true
                return
            } else if (actionId === "go.network") {
                if (filterBar.visible) filterBar.closed()
                root.applicationsMode = false
                root.networkMode = true
                return
            } else if (actionId === "network.connect") {
                if (filterBar.visible) filterBar.closed()
                root.applicationsMode = false
                root.networkMode = true
                windowServices.openConnectDialog("")
                return
            } else if (actionId === "app.preferences") {
                windowServices.openPreferences()
                return
            } else if (actionId === "go.back") {
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
                root.applicationsMode = false
                root.networkMode = false
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
                root.applicationsMode = false
                root.networkMode = false
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

    // A finished network transfer only changes what is on screen when it
    // landed in the folder being browsed; the queue itself never navigates.
    Connections {
        target: root.transferQueueController
        function onTransferCommitted(destinationFolder) {
            const destination = destinationFolder.toString()
            const current = root.navigationController.currentPath
            if (destination === current
                || destination === "file://" + current)
                root.navigationController.refresh()
        }
    }

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
    menuBar: ExportedMenuBar {
        coordinator: root.coordinator
        visible: root.inWindowMenuVisible
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
            visible: !root.applicationsMode && !root.networkMode
            navigationController: root.navigationController
            mutationController: root.mutationController
            appCoordinator: root.coordinator
            onBrowseRequested: root.activeView().focusView()
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

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.networkMode ? 2 : root.applicationsMode ? 1 : 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                PlacesSidebar {
                    Layout.preferredWidth: root.width < 680 ? 148 : 196
                    Layout.fillHeight: true
                    navigationController: root.navigationController
                    placesController: root.placesController
                    networkLocationsController: root.networkLocationsController
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

            ApplicationsView {
                objectName: "applicationsView"
                Layout.fillWidth: true
                Layout.fillHeight: true
                applicationsController: root.applicationsController
                chooserMode: root.chooserMode
            }

            NetworkHub {
                Layout.fillWidth: true
                Layout.fillHeight: true
                networkLocationsController: root.networkLocationsController
                discoveryController: root.discoveryController
                onOpenRequested: (url) => root.navigationController.navigateTo(url)
                onConnectRequested: root.coordinator.activateAction("network.connect")
                onSaveRequested: (url) => windowServices.openConnectDialog(url)
            }
        }

        FolderStatusBar {
            Layout.fillWidth: true
            visible: !root.applicationsMode && !root.networkMode
            navigationController: root.navigationController
            selection: entrySelection
            appCoordinator: root.coordinator
        }

        StatusBanners {
            Layout.fillWidth: true
            navigationController: root.navigationController
            mutationController: root.mutationController
            placesController: root.placesController
            transferQueueController: root.transferQueueController
            networkLocationsController: root.networkLocationsController
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
        transferQueueController: root.transferQueueController
        confirmTrash: root.preferencesController.confirmTrash
    }

    PropertiesDialog {
        id: propertiesDialog
        objectName: "propertiesDialog"
        controller: root.propertiesController
    }

    WindowServices {
        id: windowServices
        anchors.fill: parent
        navigationController: root.navigationController
        networkLocationsController: root.networkLocationsController
        preferencesController: root.preferencesController
        discoveryController: root.discoveryController
        mountManager: root.mountManager
    }
}
