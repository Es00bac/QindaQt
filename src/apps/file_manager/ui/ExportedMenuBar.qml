// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// The window's in-window menu authority, built from the AppShell
// coordinator's published menus. It is hidden while the global-menu export
// claims the window (composeFileManagerMenuExport flips inWindowMenuVisible).
// Extracted from Main.qml so the window stays within the source-shape budget;
// its only dependency is the coordinator.
MenuBar {
    id: root
    objectName: "appShellMenuBar"

    required property var coordinator

    Instantiator {
        model: root.coordinator.menus

        delegate: Menu {
            id: exportedMenu
            required property var modelData
            title: modelData.label

            Instantiator {
                model: exportedMenu.modelData.actions

                delegate: Action {
                    required property var modelData
                    text: modelData.label
                    enabled: modelData.enabled
                    checkable: modelData.checkable
                    checked: modelData.checked
                    shortcut: modelData.shortcut
                    onTriggered: root.coordinator.activateAction(modelData.id)
                }

                onObjectAdded: function(index, object) {
                    exportedMenu.insertAction(index, object)
                }
                onObjectRemoved: function(index, object) {
                    exportedMenu.removeAction(object)
                }
            }
        }

        onObjectAdded: function(index, object) {
            root.insertMenu(index, object)
        }
        onObjectRemoved: function(index, object) {
            root.removeMenu(object)
        }
    }
}
