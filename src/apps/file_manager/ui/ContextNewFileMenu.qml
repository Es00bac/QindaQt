// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// New File ▸ in FileContextMenu (ADR-0269): an empty file, then one entry per
// template the window's FileActions lists. Each entry opens FileActions' name
// dialog; MutationController creates the file.
Menu {
    id: menu

    required property var contextMenu
    readonly property var fileActions: menu.contextMenu.fileActions

    objectName: "contextNewFileSubMenu"
    title: qsTr("New File")
    enabled: menu.contextMenu.actionEnabled("file.new-file")

    MenuItem {
        objectName: "contextNewEmptyFileAction"
        text: qsTr("Empty File…")
        onTriggered: menu.fileActions.newFile("", qsTr("Untitled"))
    }
    MenuSeparator { visible: menu.fileActions !== null && menu.fileActions.templates.length > 0 }
    Instantiator {
        model: menu.fileActions ? menu.fileActions.templates : []
        delegate: MenuItem {
            required property var modelData
            text: qsTr("%1…").arg(modelData.name)
            onTriggered: menu.fileActions.newFile(modelData.path, modelData.fileName)
        }
        // After the empty-file entry and the separator.
        onObjectAdded: (index, object) => menu.insertItem(index + 2, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}
