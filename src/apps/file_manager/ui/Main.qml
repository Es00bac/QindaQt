// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ADR-0116: stock Qt Quick Controls only — no QindaQt.Tokens/Controls imports
// and no palette literals; appearance comes from the Qt platform theme
// (ADR-0115) with QT_QUICK_CONTROLS_STYLE=Fusion set by the session. The
// folder views are QindaTK (ADR-0270), themed from that same palette
// (ToolkitTheme).
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
    // ADR-0269: the right-click set's owners. Optional so a fixture window
    // without them still loads; their actions then do nothing.
    property var openWithController: null
    property var folderLaunchController: null
    property var fileTemplates: null
    // ADR-0270: the Details view's lazily read facts and the Columns view's
    // other columns; optional so a fixture window without them still loads.
    property var entryFacts: null
    property var columnListing: null
    // ADR-0271: makes the controllers of every tab after the first
    // (runtime/folder_navigations.h); without it the window has one tab.
    property var navigationFactory: null
    // ADR-0271: Finder, Explorer or Commander -- the user's pick, else the
    // desktop layout's.
    readonly property string fileManagerStyle: root.preferencesController.fileManagerStyle || "finder"
    // The tab the user works in (FolderPanes). Everything window-wide follows
    // it; `navigationController` is only the first tab's.
    readonly property var activeNavigation: panes.activeNavigation
    readonly property var activeSelection: panes.activeSelection

    property bool closeAuthorized: false
    property bool inWindowMenuVisible: true
    // ADR-0273: Keep in Dock, set after loading (runtime/finder_integration).
    property var dockPins: null
    // ADR-0194: the Network place's hub replaces the folder views while true.
    // Browsing to any folder leaves it. (Applications is a browsable place in
    // the ordinary views since ADR-0262, so it needs no mode of its own.)
    property bool networkMode: false
    Binding {
        target: root.activeNavigation
        property: "folderViewActive"
        value: !root.networkMode
    }
    // ADR-0165: --choose-application starts in the Applications place as a
    // workspace picker (main.cpp); ApplicationsController routes activations to
    // the compositor and a successful choice quits the picker window after
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
        target: root.activeNavigation
        // Browsing anywhere leaves the Network hub; the folder views are the
        // default surface and a Places click must land there.
        function onNavigationChanged() {
            root.networkMode = false
        }
    }

    Connections {
        target: root.activeSelection
        function onSelectedChanged() {
            root.clipboardController.selectionCount = root.activeSelection.count()
        }
    }
    onActiveSelectionChanged: root.clipboardController.selectionCount = root.activeSelection.count()

    function activeView() {
        return panes.activeViews.activeView
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
        return root.activeNavigation.currentPath
    }

    Connections {
        target: root.coordinator
        function onActionRequested(actionId) {
            const navigation = root.activeNavigation
            if (applicationsActions.handle(actionId) || fileActions.handle(actionId)
                    || panes.activeViews.handle(actionId) || quickLook.handle(actionId)) {
                return
            } else if (actionId === "go.applications" || actionId === "go.recents") {
                if (filterBar.visible) filterBar.closed()
                root.networkMode = false
                navigation.navigateTo(actionId === "go.recents"
                                      ? root.placesController.recentsLocation
                                      : root.applicationsController.location)
                return
            } else if (actionId === "go.network") {
                if (filterBar.visible) filterBar.closed()
                root.networkMode = true
                return
            } else if (actionId === "network.connect") {
                if (filterBar.visible) filterBar.closed()
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
            } else if (actionId === "go.home") {
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

    readonly property string displayedViewMode: root.activeNavigation.viewMode
    // AGENT-GUARD: a folder with a view of its own changes the mode while
    // the Explorer tree (or any sidebar row) drives navigation; keep the
    // keyboard there, or its next Return lands on the file view.
    onDisplayedViewModeChanged: Qt.callLater(() => {
        for (let item = root.activeFocusItem; item; item = item.parent) {
            if (item === placesSidebar)
                return
        }
        root.activeView().focusView()
    })

    // A finished network transfer only changes what is on screen when it
    // landed in the folder being browsed; the queue itself never navigates.
    Connections {
        target: root.transferQueueController
        function onTransferCommitted(destinationFolder) {
            const destination = destinationFolder.toString()
            const current = root.activeNavigation.currentPath
            if (destination === current
                || destination === "file://" + current)
                root.activeNavigation.refresh()
        }
    }

    Connections {
        target: root.mutationController
        function onMutationCommitted() {
            // While search results are on screen, re-run the bounded search
            // instead of refreshing: a plain refresh would silently drop the
            // guest listing the user is acting on.
            if (root.activeNavigation.guestListingActive)
                root.searchController.restart()
            else
                root.activeNavigation.refresh()
            panes.refreshOtherPane()
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
            visible: !root.networkMode
            navigationController: root.activeNavigation
            mutationController: root.mutationController
            appCoordinator: root.coordinator
            addressBar: root.fileManagerStyle === "explorer"
            onBrowseRequested: root.activeView().focusView()
        }

        CommandBar {
            Layout.fillWidth: true
            visible: !root.networkMode && root.fileManagerStyle === "explorer"
            appCoordinator: root.coordinator
            navigationController: root.activeNavigation
        }

        FilterBar {
            id: filterBar
            Layout.fillWidth: true
            visible: false
            navigationController: root.activeNavigation
            searchController: root.searchController
            onClosed: {
                root.searchController.cancel()
                if (root.activeNavigation.guestListingActive)
                    root.activeNavigation.clearGuestListing()
                root.activeNavigation.setNameFilter("")
                visible = false
                root.activeView().focusView()
            }
            onBrowseRequested: root.activeView().focusView()
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.networkMode ? 1 : 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                PlacesSidebar {
                    id: placesSidebar
                    Layout.preferredWidth: root.width < 680 ? 148 : 196
                    Layout.fillHeight: true
                    navigationController: root.activeNavigation
                    placesController: root.placesController
                    networkLocationsController: root.networkLocationsController
                    appCoordinator: root.coordinator
                    mutationController: root.mutationController
                    clipboardController: root.clipboardController
                    showFolderTree: root.fileManagerStyle === "explorer"
                    columnListing: root.columnListing
                }

                // ADR-0271: one pane or Commander's two, each with its tabs.
                FolderPanes {
                    id: panes
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    navigationController: root.navigationController
                    navigationFactory: root.navigationFactory
                    dualPane: root.fileManagerStyle === "commander"
                    appCoordinator: root.coordinator
                    mutationController: root.mutationController
                    clipboardController: root.clipboardController
                    fileActions: fileActions
                    preferencesController: root.preferencesController
                    entryFacts: root.entryFacts
                    columnListing: root.columnListing
                }
            }

            NetworkHub {
                Layout.fillWidth: true
                Layout.fillHeight: true
                networkLocationsController: root.networkLocationsController
                discoveryController: root.discoveryController
                onOpenRequested: (url) => root.activeNavigation.navigateTo(url)
                onConnectRequested: root.coordinator.activateAction("network.connect")
                onSaveRequested: (url) => windowServices.openConnectDialog(url)
            }
        }

        FolderStatusBar {
            Layout.fillWidth: true
            visible: !root.networkMode
            navigationController: root.activeNavigation
            selection: root.activeSelection
            appCoordinator: root.coordinator
        }

        FunctionKeyBar {
            Layout.fillWidth: true
            visible: !root.networkMode && panes.twoPanes
            functionKeys: panes.functionKeys
            appCoordinator: root.coordinator
        }

        StatusBanners {
            Layout.fillWidth: true
            chooserMode: root.chooserMode
            navigationController: root.activeNavigation
            mutationController: root.mutationController
            placesController: root.placesController
            transferQueueController: root.transferQueueController
            networkLocationsController: root.networkLocationsController
            openWithController: root.openWithController
            folderLaunchController: root.folderLaunchController
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
        navigationController: root.activeNavigation
        mutationController: root.mutationController
        transferQueueController: root.transferQueueController
        confirmTrash: root.preferencesController.confirmTrash
        transferTarget: panes.otherPanePath
    }

    PropertiesDialog {
        id: propertiesDialog
        objectName: "propertiesDialog"
        controller: root.propertiesController
    }

    FileActions {
        id: fileActions
        anchors.fill: parent
        navigationController: root.activeNavigation
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        propertiesController: root.propertiesController
        placesController: root.placesController
        selection: root.activeSelection
        openWithController: root.openWithController
        folderLaunchController: root.folderLaunchController
        fileTemplates: root.fileTemplates
        onPropertiesRequested: propertiesDialog.open()
    }

    ApplicationsPlaceActions {
        id: applicationsActions
        anchors.fill: parent
        applicationsController: root.applicationsController
        dockPins: root.dockPins
        navigationController: root.activeNavigation
        selection: root.activeSelection
        views: panes.activeViews
    }

    // ADR-0272: Space, Ctrl+Y and the File menu preview the selection here.
    QuickLook {
        id: quickLook
        objectName: "quickLook"
        anchors.fill: parent
        selection: root.activeSelection
        views: panes.activeViews
        navigationController: root.activeNavigation
        entryFacts: root.entryFacts
        preferencesController: root.preferencesController
    }

    // ADR-0273: org.freedesktop.FileManager1 and --select reveal entries here.
    EntryReveal {
        objectName: "entryReveal"
        navigationController: root.activeNavigation
        selection: root.activeSelection
        views: panes.activeViews
        coordinator: root.coordinator
    }

    ToolkitTheme {
        palette: root.palette
    }

    WindowServices {
        id: windowServices
        anchors.fill: parent
        navigationController: root.activeNavigation
        networkLocationsController: root.networkLocationsController
        preferencesController: root.preferencesController
        discoveryController: root.discoveryController
        mountManager: root.mountManager
    }
}
