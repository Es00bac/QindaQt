// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// The local and desktop menus share AppShell's enabled action snapshot.
Tk.MenuBar {
    id: bar
    required property var menusModel
    signal activated(string actionId)
    Instantiator {
        model: bar.menusModel
        delegate: Tk.Menu {
            id: menu
            required property var modelData
            title: modelData.label
            Instantiator {
                model: menu.modelData.actions
                delegate: Tk.MenuItem {
                    required property var modelData
                    text: modelData.label
                    shortcut: modelData.shortcut
                    enabled: modelData.enabled
                    onTriggered: bar.activated(modelData.id)
                }
                onObjectAdded: (index, object) => menu.insertItem(index, object)
                onObjectRemoved: (index, object) => menu.removeItem(object)
            }
        }
        onObjectAdded: (index, object) => bar.insertMenu(index, object)
        onObjectRemoved: (index, object) => bar.removeMenu(object)
    }
}
