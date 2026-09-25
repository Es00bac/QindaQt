// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// One tab (ADR-0271): one NavigationController with the selection, per-folder
// view settings and four views that belong to it, so a tab keeps its folder,
// history, selection and scroll position while another is in front. Shown
// while it is its pane's current tab; the folder views while the folder
// reads, the state card otherwise.
//
// AGENT-CONTRACT: FolderPanes creates and destroys tabs and is the only
// reader of `selection`, `views` and `folderViews`; Main.qml reaches the tab
// the user works in through FolderPanes.activeTab. Before this tab's
// controller is released (FolderNavigations.release()), the tab is destroyed.
StackLayout {
    id: root

    // The FolderPane this tab shows in; null once closed.
    property var pane: null
    required property var navigationController
    required property var appCoordinator
    property var mutationController: null
    property var clipboardController: null
    property var fileActions: null
    required property var preferencesController
    property var entryFacts: null
    property var columnListing: null

    readonly property alias selection: entrySelection
    readonly property alias views: views
    readonly property alias folderViews: folderViewSettings
    // The folder's own name, as the tab shows it.
    readonly property string title: {
        const crumbs = root.navigationController.breadcrumb
        return crumbs.length > 0 ? crumbs[crumbs.length - 1].name
                                 : root.navigationController.currentPath
    }

    anchors.fill: parent
    visible: root.pane !== null && root.pane.currentTab === root
    currentIndex: root.navigationController.statusKey === "ready" ? 0 : 1

    EntrySelection {
        id: entrySelection
        objectName: "entrySelection"
        navigationController: root.navigationController
    }

    FolderViewSettings {
        id: folderViewSettings
        objectName: "folderViewSettings"
        preferencesController: root.preferencesController
        navigationController: root.navigationController
    }

    FolderViewStack {
        id: views
        navigationController: root.navigationController
        selection: entrySelection
        appCoordinator: root.appCoordinator
        mutationController: root.mutationController
        clipboardController: root.clipboardController
        fileActions: root.fileActions
        preferencesController: root.preferencesController
        folderViews: folderViewSettings
        entryFacts: root.entryFacts
        columnListing: root.columnListing
    }

    StatePane {
        statusKey: root.navigationController.statusKey
        statusMessage: root.navigationController.statusMessage
        onRetryRequested: root.navigationController.refresh()
    }
}
