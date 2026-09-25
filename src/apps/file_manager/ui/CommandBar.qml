// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Explorer style's compact command bar (ADR-0271): New, Cut, Copy,
// Paste, Rename, Delete, Sort and View. Every button runs its catalog action
// through the coordinator, exactly as its menu item does, and is available
// exactly when that action is -- the bar keeps no state of its own.
ToolBar {
    id: root
    objectName: "commandBar"

    required property var appCoordinator
    required property var navigationController

    padding: 2

    // The action's enabled state as the coordinator publishes it.
    function available(actionId) {
        for (const menu of root.appCoordinator.menus) {
            for (const action of menu.actions) {
                if (action.id === actionId)
                    return action.enabled === true
            }
        }
        return false
    }

    // An inline component has its own scope, so each button is handed the
    // bar it belongs to.
    component CommandButton: ToolButton {
        id: button
        required property string iconName
        required property var bar
        property string actionId: ""
        display: button.bar.width >= 640 ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
        icon.source: "image://theme-icons/" + button.iconName + "-symbolic"
            + "?color=" + encodeURIComponent(button.palette.buttonText.toString())
        icon.width: 18
        icon.height: 18
        enabled: button.actionId.length === 0 || button.bar.available(button.actionId)
        ToolTip.visible: button.display === AbstractButton.IconOnly && (button.hovered || button.activeFocus)
        ToolTip.delay: 600
        ToolTip.text: button.text
        Accessible.name: button.text
        onClicked: {
            if (button.actionId.length > 0)
                button.bar.appCoordinator.activateAction(button.actionId)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 2

        CommandButton {
            bar: root
            objectName: "commandNewButton"
            iconName: "document-new"
            text: qsTr("New")
            enabled: root.available("file.new-folder") || root.available("file.new-file")
            onClicked: newMenu.popup()
            Menu {
                id: newMenu
                MenuItem {
                    text: qsTr("Folder")
                    enabled: root.available("file.new-folder")
                    onTriggered: root.appCoordinator.activateAction("file.new-folder")
                }
                MenuItem {
                    text: qsTr("File…")
                    enabled: root.available("file.new-file")
                    onTriggered: root.appCoordinator.activateAction("file.new-file")
                }
            }
        }
        ToolSeparator {}
        CommandButton {
            bar: root
            objectName: "commandCutButton"
            iconName: "edit-cut"
            text: qsTr("Cut")
            actionId: "edit.cut"
        }
        CommandButton {
            bar: root
            objectName: "commandCopyButton"
            iconName: "edit-copy"
            text: qsTr("Copy")
            actionId: "edit.copy"
        }
        CommandButton {
            bar: root
            objectName: "commandPasteButton"
            iconName: "edit-paste"
            text: qsTr("Paste")
            actionId: "edit.paste"
        }
        CommandButton {
            bar: root
            objectName: "commandRenameButton"
            iconName: "edit-rename"
            text: qsTr("Rename")
            actionId: "file.rename"
        }
        CommandButton {
            bar: root
            objectName: "commandDeleteButton"
            iconName: "edit-delete"
            text: qsTr("Delete")
            actionId: "file.trash"
        }
        ToolSeparator {}
        CommandButton {
            bar: root
            objectName: "commandSortButton"
            iconName: "view-sort"
            text: qsTr("Sort")
            onClicked: sortMenu.popup()
            Menu {
                id: sortMenu
                Repeater {
                    model: [{ "key": "name", "text": qsTr("Name"), "action": "view.sort-name" },
                        { "key": "size", "text": qsTr("Size"), "action": "view.sort-size" },
                        { "key": "kind", "text": qsTr("Kind"), "action": "view.sort-kind" },
                        { "key": "modified", "text": qsTr("Date Modified"), "action": "view.sort-modified" }]
                    MenuItem {
                        required property var modelData
                        text: modelData.text
                        checkable: true
                        checked: root.navigationController.sortColumn === modelData.key
                        onTriggered: root.appCoordinator.activateAction(modelData.action)
                    }
                }
            }
        }
        CommandButton {
            bar: root
            objectName: "commandViewButton"
            iconName: "view-list-details"
            text: qsTr("View")
            onClicked: viewMenu.popup()
            Menu {
                id: viewMenu
                Repeater {
                    model: [{ "mode": "grid", "text": qsTr("Icons"), "action": "view.grid-mode" },
                        { "mode": "list", "text": qsTr("Details"), "action": "view.details-mode" },
                        { "mode": "columns", "text": qsTr("Columns"), "action": "view.columns-mode" },
                        { "mode": "gallery", "text": qsTr("Gallery"), "action": "view.gallery-mode" }]
                    MenuItem {
                        required property var modelData
                        text: modelData.text
                        checkable: true
                        checked: root.navigationController.viewMode === modelData.mode
                        onTriggered: root.appCoordinator.activateAction(modelData.action)
                    }
                }
            }
        }
        Item { Layout.fillWidth: true }
    }
}
