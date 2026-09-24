// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// The right-click set's window actions (ADR-0269), kept out of Main.qml:
// Open, Open With, Open in New Window, Duplicate, Make Link, Copy Path,
// Compress, Extract, Add to Sidebar, Get Info, Delete Permanently, Put Back,
// New File, Open Terminal Here and Sort By. Availability is decided in C++
// (file_manager_item_actions); this item carries each action out through a
// C++ controller, owns the Open With chooser and the New File and Delete
// Permanently dialogs, and answers FileContextMenu's per-selection questions.
// AGENT-NOTE: W11 moves the views to QindaTK; keep behaviour here and in the
// controllers, never in a view.
Item {
    id: root

    required property var navigationController
    required property var mutationController
    required property var clipboardController
    required property var propertiesController
    required property var placesController
    // The window's EntrySelection (Main.qml's instance).
    required property var selection
    // Optional: fixture windows may leave these null; the actions that need
    // one then do nothing and the menu hides them.
    property var openWithController: null
    property var folderLaunchController: null
    property var fileTemplates: null

    // Main opens its PropertiesDialog when a Get Info request is ready.
    signal propertiesRequested()

    // The home Trash's files/ folder, as the Places list names it.
    readonly property string trashPath: {
        const places = root.placesController ? root.placesController.places : []
        for (const place of places) {
            if (place.id === "trash")
                return place.path
        }
        return ""
    }
    readonly property bool inTrash: root.trashPath.length > 0
        && root.navigationController.currentPath === root.trashPath
    readonly property var templates: root.fileTemplates ? root.fileTemplates.templates : []

    function selectedEntries() {
        return root.selection.selectedEntries()
    }

    // What the selection holds, for the context menu's visibility rules.
    function describeSelection() {
        const entries = root.selectedEntries()
        let files = 0
        let folders = 0
        let archives = 0
        for (const entry of entries) {
            if (entry.isDirectory) {
                ++folders
            } else {
                ++files
                if (root.mutationController.isExtractable(entry.path))
                    ++archives
            }
        }
        return { "files": files, "folders": folders, "archives": archives }
    }

    function filePaths(entries) {
        return entries.filter(entry => !entry.isDirectory).map(entry => entry.path)
    }

    // Open With ▸'s entries for the current selection (files only).
    function openWithCandidates() {
        const paths = root.filePaths(root.selectedEntries())
        return root.openWithController && paths.length > 0
            ? root.openWithController.candidatesFor(paths) : []
    }

    function openWith(desktopId) {
        const paths = root.filePaths(root.selectedEntries())
        if (root.openWithController && paths.length > 0)
            root.openWithController.openWith(desktopId, paths)
    }

    function refreshTemplates() {
        if (root.fileTemplates)
            root.fileTemplates.refresh()
    }

    // New File ▸: an empty file (templatePath "") or a copy of a template,
    // named in a dialog that starts from `suggestedName`.
    function newFile(templatePath, suggestedName) {
        newFileDialog.templatePath = templatePath
        newFileName.text = suggestedName
        newFileDialog.open()
    }

    // The background menu's Get Info always describes the browsed folder,
    // whatever stays selected (the same reason background Paste overrides its
    // destination).
    function showFolderInfo() {
        root.propertiesController.inspectFolder(root.navigationController.currentPath)
        if (root.propertiesController.active)
            root.propertiesRequested()
    }

    function open(entries) {
        const navigation = root.navigationController
        if (entries.length === 1) {
            // A double-click's path: a folder opens here, a file with its default.
            navigation.activate(root.selection.indexOfKey(root.selection.key(entries[0])))
            return
        }
        // Several items: each file opens with its default application and
        // each folder in a window of its own, as Finder does.
        for (const entry of entries) {
            if (entry.isDirectory) {
                if (root.folderLaunchController)
                    root.folderLaunchController.openInNewWindow(entry)
            } else {
                navigation.activate(root.selection.indexOfKey(root.selection.key(entry)))
            }
        }
    }

    // Returns true when actionId is one of this item's and was handled here;
    // every other action stays with Main.qml's dispatch.
    function handle(actionId) {
        const entries = root.selectedEntries()
        const sortKeys = { "view.sort-name": "name", "view.sort-size": "size",
                           "view.sort-kind": "kind", "view.sort-modified": "modified" }
        if (sortKeys[actionId] !== undefined) {
            // Choosing the active sort again reverses it, as a column header does.
            root.navigationController.setSortColumn(sortKeys[actionId])
            return true
        }
        switch (actionId) {
        case "file.properties":
            // Get Info: the selection, or the browsed folder when nothing is
            // selected (the Applications place answers before this, ADR-0262).
            if (entries.length > 0)
                root.propertiesController.inspect(entries)
            else
                root.propertiesController.inspectFolder(root.navigationController.currentPath)
            if (root.propertiesController.active)
                root.propertiesRequested()
            return true
        case "file.open":
            root.open(entries)
            return true
        case "file.open-with":
            if (root.openWithController && root.filePaths(entries).length > 0)
                openWithDialog.openFor(root.filePaths(entries))
            return true
        case "file.open-new-window":
            for (const entry of entries.filter(item => item.isDirectory)) {
                if (root.folderLaunchController)
                    root.folderLaunchController.openInNewWindow(entry)
            }
            return true
        case "file.duplicate":
            root.mutationController.duplicateItems(entries)
            return true
        case "file.make-link":
            root.mutationController.makeLinks(entries)
            return true
        case "edit.copy-path":
            root.clipboardController.copyPathsAsText(entries)
            return true
        case "file.compress":
            root.mutationController.compressItems(entries)
            return true
        case "file.extract":
            root.mutationController.extractItems(
                entries.filter(entry => root.mutationController.isExtractable(entry.path)))
            return true
        case "file.add-to-sidebar":
            for (const entry of entries.filter(item => item.isDirectory))
                root.placesController.addBookmark(entry.name, entry.path)
            return true
        case "file.delete":
            // ADR-0269: always confirmed, whatever the Trash preference says.
            if (entries.length > 0) {
                deleteConfirmationDialog.entries = entries
                deleteConfirmationDialog.open()
            }
            return true
        case "file.put-back":
            root.mutationController.putBackItems(entries)
            return true
        case "file.new-file":
            root.refreshTemplates()
            root.newFile("", qsTr("Untitled"))
            return true
        case "file.open-terminal":
            if (root.folderLaunchController)
                root.folderLaunchController.openTerminal(root.navigationController.currentPath)
            return true
        }
        return false
    }

    OpenWithDialog {
        id: openWithDialog
        controller: root.openWithController
    }

    Dialog {
        id: newFileDialog
        objectName: "newFileDialog"
        property string templatePath: ""
        anchors.centerIn: parent
        width: Math.min(440, root.width - 32)
        modal: true
        title: newFileDialog.templatePath.length > 0
            ? qsTr("Create a file from a template") : qsTr("Create an empty file")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: root.mutationController.createFile(root.navigationController.currentPath,
                                                       newFileName.text,
                                                       newFileDialog.templatePath)

        TextField {
            id: newFileName
            objectName: "newFileNameField"
            width: parent.width
            placeholderText: qsTr("File name")
            Accessible.name: qsTr("New file name")
        }
    }

    Dialog {
        id: deleteConfirmationDialog
        objectName: "deleteConfirmationDialog"
        property var entries: []
        anchors.centerIn: parent
        width: Math.min(560, root.width - 32)
        modal: true
        title: deleteConfirmationDialog.entries.length > 1
            ? qsTr("Delete %1 items permanently?").arg(deleteConfirmationDialog.entries.length)
            : qsTr("Delete this item permanently?")
        standardButtons: Dialog.Yes | Dialog.Cancel
        // Cleared only after use: closed() can arrive before accepted().
        onAccepted: {
            root.mutationController.deleteItems(deleteConfirmationDialog.entries)
            deleteConfirmationDialog.entries = []
        }
        onRejected: deleteConfirmationDialog.entries = []

        Label {
            width: parent.width
            text: qsTr("This cannot be undone: the items do not go to Trash.") + "\n\n"
                + deleteConfirmationDialog.entries.slice(0, 5).map(entry => entry.name).join("\n")
                + (deleteConfirmationDialog.entries.length > 5
                   ? qsTr("\n…and %1 more").arg(deleteConfirmationDialog.entries.length - 5) : "")
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }
}
