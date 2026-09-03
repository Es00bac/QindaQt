// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

Item {
    id: root

    required property var navigationController
    required property var mutationController
    property var selectedEntry: null
    property string destinationKind: "copy"

    function dispatch(actionId, entry) {
        if (actionId === "file.new-folder") {
            newFolderName.text = ""
            newFolderDialog.open()
        } else if (actionId === "file.rename") {
            if (!entry) return
            selectedEntry = entry
            renameName.text = entry.name
            renameDialog.open()
        } else if (actionId === "file.copy" || actionId === "file.move") {
            if (!entry) return
            selectedEntry = entry
            destinationKind = actionId === "file.copy" ? "copy" : "move"
            destinationPath.text = navigationController.currentPath + "/" + entry.name
            destinationDialog.open()
        } else if (actionId === "file.trash") {
            if (!entry) return
            selectedEntry = entry
            trashConfirmationDialog.open()
        } else if (actionId === "file.restore-last") {
            mutationController.restoreLast()
        } else if (actionId === "file.empty-trash") {
            emptyTrashConfirmationDialog.open()
        } else if (actionId === "edit.undo") {
            mutationController.undo()
        } else if (actionId === "operation.cancel") {
            mutationController.cancel()
        }
    }

    T.Dialog {
        id: newFolderDialog
        objectName: "newFolderDialog"
        anchors.centerIn: parent
        width: Math.min(440, root.width - Tokens.space["4"] * 2)
        modal: true
        title: qsTr("Create a new folder")
        standardButtons: T.Dialog.Ok | T.Dialog.Cancel
        onAccepted: root.mutationController.createFolder(
            root.navigationController.currentPath, newFolderName.text)

        T.TextField {
            id: newFolderName
            objectName: "newFolderNameField"
            width: parent.width
            placeholderText: qsTr("Folder name")
            Accessible.name: qsTr("New folder name")
        }
    }

    T.Dialog {
        id: renameDialog
        objectName: "renameDialog"
        anchors.centerIn: parent
        width: Math.min(440, root.width - Tokens.space["4"] * 2)
        modal: true
        title: qsTr("Rename selected item")
        standardButtons: T.Dialog.Ok | T.Dialog.Cancel
        onAccepted: {
            if (root.selectedEntry)
                root.mutationController.renameItem(root.selectedEntry.path,
                    renameName.text, root.selectedEntry)
        }

        T.TextField {
            id: renameName
            objectName: "renameNameField"
            width: parent.width
            Accessible.name: qsTr("New item name")
        }
    }

    T.Dialog {
        id: destinationDialog
        objectName: "destinationDialog"
        anchors.centerIn: parent
        width: Math.min(560, root.width - Tokens.space["4"] * 2)
        modal: true
        title: root.destinationKind === "copy" ? qsTr("Copy to local path")
                                                 : qsTr("Move to local path")
        standardButtons: T.Dialog.Ok | T.Dialog.Cancel
        onAccepted: {
            if (!root.selectedEntry) return
            if (root.destinationKind === "copy")
                root.mutationController.copyItem(root.selectedEntry.path,
                    destinationPath.text, root.selectedEntry)
            else
                root.mutationController.moveItem(root.selectedEntry.path,
                    destinationPath.text, root.selectedEntry)
        }

        T.TextField {
            id: destinationPath
            objectName: "destinationPathField"
            width: parent.width
            Accessible.name: qsTr("Absolute destination path")
        }
    }

    T.Dialog {
        id: trashConfirmationDialog
        objectName: "trashConfirmationDialog"
        anchors.centerIn: parent
        modal: true
        title: qsTr("Move selected item to Trash?")
        standardButtons: T.Dialog.Yes | T.Dialog.Cancel
        onAccepted: {
            if (root.selectedEntry)
                root.mutationController.trashItem(root.selectedEntry.path,
                                                   root.selectedEntry)
        }

        T.Label {
            text: root.selectedEntry ? qsTr("Move “%1” to the recoverable home Trash.")
                                           .arg(root.selectedEntry.name) : ""
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }

    T.Dialog {
        id: emptyTrashConfirmationDialog
        objectName: "emptyTrashConfirmationDialog"
        anchors.centerIn: parent
        modal: true
        title: qsTr("Permanently empty Trash?")
        standardButtons: T.Dialog.Yes | T.Dialog.Cancel
        onAccepted: root.mutationController.emptyTrash()

        T.Label {
            text: qsTr("Every item in the home Trash will be permanently removed. This cannot be undone.")
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }
}
