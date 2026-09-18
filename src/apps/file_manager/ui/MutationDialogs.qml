// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property var navigationController
    required property var mutationController
    required property var transferQueueController
    property var selectedEntry: null
    property var selectedItems: []
    property string destinationKind: "copy"

    // entries is the selection snapshot from the active view: an array of
    // entry maps (possibly empty). Single-item dispatches keep the existing
    // undo/restore-token-bearing invokables; multi-item dispatches use the
    // serialized batch variants, which intentionally carry no undo request.
    function dispatch(actionId, entries) {
        const selection = entries && entries.length ? entries : []
        if (actionId === "file.new-folder") {
            newFolderName.text = ""
            newFolderDialog.open()
        } else if (actionId === "file.rename") {
            if (selection.length < 1) return
            selectedEntry = selection[0]
            renameName.text = selectedEntry.name
            renameDialog.open()
        } else if (actionId === "file.copy" || actionId === "file.move") {
            if (selection.length < 1) return
            // ADR-0195: a remote multi-selection no longer fails closed here.
            // The accept handler dispatches strictly by the route C++ names
            // for the exact (sources, destination) pair, and that router can
            // never name the ADR-0155/0156 one-child owner for more than one
            // source -- so the one-child contract is now enforced where the
            // dispatch happens rather than by refusing to open the dialog.
            selectedItems = selection
            selectedEntry = selection[0]
            destinationKind = actionId === "file.copy" ? "copy" : "move"
            destinationPath.text = selection.length > 1
                ? navigationController.currentPath
                : navigationController.currentPath + "/" + selectedEntry.name
            destinationDialog.open()
        } else if (actionId === "file.trash") {
            if (selection.length < 1) return
            selectedItems = selection
            selectedEntry = selection[0]
            trashConfirmationDialog.open()
        } else if (actionId === "file.restore-last") {
            mutationController.restoreLast()
        } else if (actionId === "file.empty-trash") {
            emptyTrashConfirmationDialog.open()
        } else if (actionId === "edit.undo") {
            mutationController.undo()
        } else if (actionId === "operation.cancel") {
            // Route to the active owner (review P1 repair): an in-flight
            // remote copy or move retires through the injected collaborator
            // (quiet KIO kill, generation-fenced); otherwise the local
            // mutation backend keeps the request.
            if (root.navigationController.remoteCopyBusy)
                root.navigationController.cancelRemoteCopy()
            else if (root.navigationController.remoteMoveBusy)
                root.navigationController.cancelRemoteMove()
            else if (root.transferQueueController.busy)
                root.transferQueueController.cancelAll()
            else
                root.mutationController.cancel()
        }
    }

    Dialog {
        id: newFolderDialog
        objectName: "newFolderDialog"
        anchors.centerIn: parent
        width: Math.min(440, root.width - 32)
        modal: true
        title: qsTr("Create a new folder")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            // ADR-0154: while browsing remote, New Folder goes through the
            // navigation controller's injected KIO creator; locally it
            // stays on the identity-checked local mutation controller.
            if (root.navigationController.remoteActive)
                root.navigationController.createRemoteFolder(newFolderName.text)
            else
                root.mutationController.createFolder(
                    root.navigationController.currentPath, newFolderName.text)
        }

        TextField {
            id: newFolderName
            objectName: "newFolderNameField"
            width: parent.width
            placeholderText: qsTr("Folder name")
            Accessible.name: qsTr("New folder name")
        }
    }

    Dialog {
        id: renameDialog
        objectName: "renameDialog"
        anchors.centerIn: parent
        width: Math.min(440, root.width - 32)
        modal: true
        title: qsTr("Rename selected item")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            if (!root.selectedEntry)
                return
            // ADR-0153: while browsing remote, Rename goes through the
            // navigation controller's injected KIO renamer; locally it
            // stays on the identity-checked local mutation controller.
            if (root.navigationController.remoteActive)
                root.navigationController.renameRemoteEntry(root.selectedEntry.path,
                    renameName.text)
            else
                root.mutationController.renameItem(root.selectedEntry.path,
                    renameName.text, root.selectedEntry)
        }

        TextField {
            id: renameName
            objectName: "renameNameField"
            width: parent.width
            Accessible.name: qsTr("New item name")
        }
    }

    Dialog {
        id: destinationDialog
        objectName: "destinationDialog"
        anchors.centerIn: parent
        width: Math.min(560, root.width - 32)
        modal: true
        title: root.selectedItems.length > 1
            ? (root.destinationKind === "copy"
               ? qsTr("Copy %1 items into folder").arg(root.selectedItems.length)
               : qsTr("Move %1 items into folder").arg(root.selectedItems.length))
            : (root.destinationKind === "copy"
               ? qsTr("Copy to folder or address")
               : qsTr("Move to folder or address"))
        standardButtons: Dialog.Ok | Dialog.Cancel
        // ADR-0195: one owner per request, named by C++ for this exact pair
        // of source list and destination. "local" keeps the identity-checked
        // local mutation controller, "remote-child" the ADR-0155/0156
        // one-child remote path, "queue" the network transfer queue, and
        // "refuse" is handed to the queue so its own banner states why.
        onAccepted: {
            if (root.selectedItems.length < 1) return
            const sources = root.selectedItems.map(entry => entry.path)
            const route = root.transferQueueController.routeName(
                sources, destinationPath.text)
            if (route === "queue" || route === "refuse") {
                root.transferQueueController.enqueue(
                    sources, destinationPath.text, root.destinationKind)
                return
            }
            if (route === "remote-child") {
                if (root.destinationKind === "copy")
                    root.navigationController.copyRemoteChild(sources[0],
                        destinationPath.text)
                else
                    root.navigationController.moveRemoteChild(sources[0],
                        destinationPath.text)
                return
            }
            if (root.selectedItems.length > 1) {
                if (root.destinationKind === "copy")
                    root.mutationController.copyItemsTo(root.selectedItems,
                        destinationPath.text)
                else
                    root.mutationController.moveItemsTo(root.selectedItems,
                        destinationPath.text)
                return
            }
            if (!root.selectedEntry) return
            if (root.destinationKind === "copy")
                root.mutationController.copyItem(root.selectedEntry.path,
                    destinationPath.text, root.selectedEntry)
            else
                root.mutationController.moveItem(root.selectedEntry.path,
                    destinationPath.text, root.selectedEntry)
        }

        TextField {
            id: destinationPath
            objectName: "destinationPathField"
            width: parent.width
            Accessible.name: qsTr("Destination folder path or sftp/smb address")
        }
    }

    Dialog {
        id: trashConfirmationDialog
        objectName: "trashConfirmationDialog"
        anchors.centerIn: parent
        width: Math.min(560, root.width - 32)
        modal: true
        title: root.selectedItems.length > 1
            ? qsTr("Move %1 selected items to Trash?").arg(root.selectedItems.length)
            : qsTr("Move selected item to Trash?")
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: {
            if (root.selectedItems.length > 1) {
                root.mutationController.trashItems(root.selectedItems)
            } else if (root.selectedEntry) {
                root.mutationController.trashItem(root.selectedEntry.path,
                                                   root.selectedEntry)
            }
        }

        Label {
            text: root.selectedItems.length > 1
                ? qsTr("Move %1 selected items to the recoverable home Trash. Batch trash is not covered by Restore Last.")
                      .arg(root.selectedItems.length) + "\n\n"
                  + root.selectedItems.slice(0, 5).map(entry => entry.name).join("\n")
                  + (root.selectedItems.length > 5
                     ? qsTr("\n…and %1 more").arg(root.selectedItems.length - 5) : "")
                : (root.selectedEntry ? qsTr("Move “%1” to the recoverable home Trash.")
                                           .arg(root.selectedEntry.name) : "")
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }

    Dialog {
        id: emptyTrashConfirmationDialog
        objectName: "emptyTrashConfirmationDialog"
        anchors.centerIn: parent
        width: Math.min(560, root.width - 32)
        modal: true
        title: qsTr("Permanently empty Trash?")
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: root.mutationController.emptyTrash()

        Label {
            text: qsTr("Every item in the home Trash will be permanently removed. This cannot be undone.")
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }
}
