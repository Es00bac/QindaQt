// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.AppShell 1.0
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

ApplicationShell {
    id: root

    required property var navigationController
    required property var mutationController
    required property var placesController

    initialFocusItem: toolbar.primaryFocusItem
    width: 900
    height: 600
    minimumWidth: 480
    minimumHeight: 320

    function activeView() {
        return root.navigationController.viewMode === "grid" ? entryGrid : entryList
    }

    Connections {
        target: root.coordinator
        function onActionRequested(actionId) {
            const navigation = root.navigationController
            if (actionId === "view.show-hidden") {
                navigation.setShowHidden(!navigation.showHidden)
            } else if (actionId === "view.grid-mode") {
                navigation.setViewMode(navigation.viewMode === "grid" ? "list" : "grid")
            } else if (actionId === "view.focus-location") {
                locationBar.visible = true
                locationBar.activate()
            } else if (actionId === "edit.select-all") {
                root.activeView().selectAll()
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

    Connections {
        target: root.mutationController
        function onMutationCommitted() {
            root.navigationController.refresh()
        }
    }

    Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Toolbar {
                id: toolbar
                Layout.fillWidth: true
                navigationController: root.navigationController
                mutationController: root.mutationController
                appCoordinator: root.coordinator
            }

            LocationBar {
                id: locationBar
                Layout.fillWidth: true
                visible: false
                navigationController: root.navigationController
                onClosed: visible = false
            }

            Breadcrumb {
                Layout.fillWidth: true
                visible: !locationBar.visible
                navigationController: root.navigationController
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                PlacesSidebar {
                    Layout.fillHeight: true
                    navigationController: root.navigationController
                    placesController: root.placesController
                    appCoordinator: root.coordinator
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.navigationController.statusKey === "ready" ? 0 : 1

                    StackLayout {
                        currentIndex: root.navigationController.viewMode === "grid" ? 1 : 0

                        EntryList {
                            id: entryList
                            navigationController: root.navigationController
                            appCoordinator: root.coordinator
                        }

                        EntryGrid {
                            id: entryGrid
                            navigationController: root.navigationController
                            appCoordinator: root.coordinator
                        }
                    }

                    StatePane {
                        statusKey: root.navigationController.statusKey
                        statusMessage: root.navigationController.statusMessage
                        onRetryRequested: root.navigationController.refresh()
                    }
                }
            }

            Qinda.StateCard {
                objectName: "mutationProgressCard"
                Layout.fillWidth: true
                visible: root.mutationController.busy
                status: Qinda.StateCard.Busy
                title: qsTr("File operation in progress")
                message: root.mutationController.progressText
                actionText: qsTr("Cancel")
                accessibleDescription: qsTr("%1. Progress %2 percent")
                    .arg(root.mutationController.progressText)
                    .arg(root.mutationController.progressValue)
                onActionTriggered: root.mutationController.cancel()
            }

            Qinda.StateCard {
                objectName: "mutationFailureCard"
                Layout.fillWidth: true
                visible: root.mutationController.failureCode !== "none"
                status: Qinda.StateCard.Error
                title: qsTr("File operation failed: %1")
                    .arg(root.mutationController.failureCode)
                message: root.mutationController.failureMessage
                actionText: qsTr("Dismiss")
                onActionTriggered: root.mutationController.clearFailure()
            }

            Qinda.StateCard {
                objectName: "mutationResultCard"
                Layout.fillWidth: true
                visible: !root.mutationController.busy
                    && root.mutationController.failureCode === "none"
                    && root.mutationController.resultText.length > 0
                status: Qinda.StateCard.Success
                title: root.mutationController.resultText
                message: root.mutationController.canRestore
                    ? qsTr("The most recently trashed item can be restored.") : ""
                actionText: root.mutationController.canUndo ? qsTr("Undo") : ""
                onActionTriggered: root.mutationController.undo()
            }

            Qinda.StateCard {
                objectName: "launchErrorBanner"
                Layout.fillWidth: true
                visible: root.navigationController.launchError.length > 0
                status: Qinda.StateCard.Warning
                title: qsTr("Couldn't open the file")
                message: root.navigationController.launchError
                actionText: qsTr("Dismiss")
                onActionTriggered: root.navigationController.clearLaunchError()
            }

            Qinda.StateCard {
                objectName: "bookmarkStoreBanner"
                Layout.fillWidth: true
                visible: root.placesController.storeError.length > 0
                status: Qinda.StateCard.Warning
                title: qsTr("Bookmark storage problem")
                message: root.placesController.storeError
                actionText: qsTr("Dismiss")
                onActionTriggered: root.placesController.clearStoreError()
            }
        }

        MutationDialogs {
            id: mutationDialogs
            anchors.fill: parent
            navigationController: root.navigationController
            mutationController: root.mutationController
        }
    }
}
