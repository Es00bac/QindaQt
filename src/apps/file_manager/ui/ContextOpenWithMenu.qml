// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Open With ▸ in FileContextMenu (ADR-0269): the recommended applications the
// menu looked up when it opened (default first and marked), then Other
// Application…, which opens FileActions' chooser through the catalog.
Menu {
    id: menu

    required property var contextMenu

    objectName: "contextOpenWithSubMenu"
    title: qsTr("Open With")

    Instantiator {
        model: menu.contextMenu.openWithCandidates
        delegate: MenuItem {
            required property var modelData
            text: modelData.isDefault ? qsTr("%1 (default)").arg(modelData.name) : modelData.name
            icon.name: modelData.iconName
            onTriggered: menu.contextMenu.fileActions.openWith(modelData.id)
        }
        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
    MenuSeparator { visible: menu.contextMenu.openWithCandidates.length > 0 }
    ContextActionItem {
        objectName: "contextOpenWithOtherAction"
        contextMenu: menu.contextMenu
        actionId: "file.open-with"
        text: qsTr("Other Application…")
    }
}
