// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every banner the window stacks below the folder view, in one place:
// local file operations, the bounded local launch, the bookmark store, and
// the network transfer queue and its refusals. Presentation only -- each
// banner reads one controller property and calls back into that same
// controller. Extracted from Main.qml so the window stays within the
// source-shape budget as surfaces are added.
ColumnLayout {
    id: root

    required property var navigationController
    required property var mutationController
    required property var placesController
    required property var transferQueueController
    required property var networkLocationsController

    spacing: 0

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

    TransferQueueBanner {
        Layout.fillWidth: true
        transferQueueController: root.transferQueueController
    }

    StatusBanner {
        objectName: "transferRefusalBanner"
        Layout.fillWidth: true
        visible: root.transferQueueController.refusal.length > 0
        title: qsTr("That transfer cannot start")
        message: root.transferQueueController.refusal
        actionText: qsTr("Dismiss")
        onActionTriggered: root.transferQueueController.clearRefusal()
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

    // The hub carries its own copy of this banner; this one is what makes a
    // saved-location write failure visible while an ordinary folder is open.
    StatusBanner {
        objectName: "networkLocationsStoreBanner"
        Layout.fillWidth: true
        visible: root.networkLocationsController.storeError.length > 0
        title: qsTr("Saved network locations could not be updated")
        message: root.networkLocationsController.storeError
        actionText: qsTr("Dismiss")
        onActionTriggered: root.networkLocationsController.clearStoreError()
    }
}
