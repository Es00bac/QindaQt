// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// The window's four views of the browsed folder (ADR-0270): Icons, Details,
// Columns and Gallery, one shown at a time by NavigationController.viewMode.
// Every view projects the same listing and the same EntrySelection, so every
// place -- folders, search results, network locations, Trash, Applications
// (ADR-0262) -- works in every view.
//
// AGENT-CONTRACT: each view offers focusItem, focusView(), currentEntry(),
// selectedEntries(), selectAll(), activateCurrent(), revealIndex(entryIndex)
// and the zoomRequested(steps) signal. Main.qml, EntryReveal and
// ApplicationsPlaceActions reach "the view the user sees" only through
// `activeView`; the entries themselves, the current entry and the selection
// are EntrySelection's (entries, currentIndex, selectedEntries()). Later
// features (tabs, Quick Look, type-to-select) build on these hooks, not on a
// view's internals.
StackLayout {
    id: root

    required property var navigationController
    required property var selection
    required property var appCoordinator
    property var mutationController: null
    property var clipboardController: null
    property var fileActions: null
    property var preferencesController: null
    // FolderViewSettings (the per-folder view state), EntryFacts and
    // ColumnListing; fixture windows may leave them null.
    property var folderViews: null
    property var entryFacts: null
    property var columnListing: null

    readonly property var modes: ["grid", "list", "columns", "gallery"]
    readonly property int modeIndex: Math.max(0, modes.indexOf(navigationController.viewMode))
    readonly property var activeView: [icons, details, columns, gallery][modeIndex]
    readonly property alias detailsView: details
    readonly property bool showExtensions: preferencesController
        ? preferencesController.showExtensions === true : true

    currentIndex: modeIndex

    // The View menu's commands this stack carries out; Main.qml asks here
    // before its own dispatch. Returns true when it handled `actionId`.
    function handle(actionId) {
        const navigation = root.navigationController
        const modes = { "view.grid-mode": "grid", "view.details-mode": "list",
                        "view.columns-mode": "columns", "view.gallery-mode": "gallery" }
        const groups = { "view.group-none": "none", "view.group-kind": "kind",
                         "view.group-date": "date", "view.group-size": "size" }
        if (modes[actionId] !== undefined) {
            navigation.setViewMode(modes[actionId])
            return true
        }
        if (groups[actionId] !== undefined) {
            navigation.setGroupBy(groups[actionId])
            return true
        }
        if (actionId === "view.show-columns") {
            details.openColumnChooser()
            return true
        }
        if (actionId === "view.use-as-defaults") {
            if (root.folderViews)
                root.folderViews.useAsDefaults()
            return true
        }
        return false
    }

    IconsView {
        id: icons
        iconSize: root.navigationController.iconSize
        onZoomRequested: (steps) => root.navigationController.zoomBy(steps)
        selection: root.selection
        navigationController: root.navigationController
        appCoordinator: root.appCoordinator
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        fileActions: root.fileActions
        showExtensions: root.showExtensions
    }

    DetailsView {
        id: details
        iconSize: root.navigationController.iconSize
        onZoomRequested: (steps) => root.navigationController.zoomBy(steps)
        selection: root.selection
        navigationController: root.navigationController
        appCoordinator: root.appCoordinator
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        fileActions: root.fileActions
        folderViews: root.folderViews
        preferencesController: root.preferencesController
        entryFacts: root.entryFacts
    }

    ColumnsView {
        id: columns
        active: root.modeIndex === 2
        iconSize: root.navigationController.iconSize
        onZoomRequested: (steps) => root.navigationController.zoomBy(steps)
        selection: root.selection
        navigationController: root.navigationController
        appCoordinator: root.appCoordinator
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        fileActions: root.fileActions
        preferencesController: root.preferencesController
        entryFacts: root.entryFacts
        columnListing: root.columnListing
        folderViews: root.folderViews
    }

    GalleryView {
        id: gallery
        active: root.modeIndex === 3
        iconSize: root.navigationController.iconSize
        onZoomRequested: (steps) => root.navigationController.zoomBy(steps)
        selection: root.selection
        navigationController: root.navigationController
        appCoordinator: root.appCoordinator
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        fileActions: root.fileActions
        preferencesController: root.preferencesController
        entryFacts: root.entryFacts
    }
}
