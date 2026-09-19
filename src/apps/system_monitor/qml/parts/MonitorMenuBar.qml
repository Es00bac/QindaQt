// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk

// The in-window menu, built from the same AppShell action snapshot the
// desktop's global menu is built from -- so the two can never drift, and
// enabled/checked state is decided once.
//
// AGENT-GUARD: this is shown only when the desktop is NOT hosting the menu.
// The window's `inWindowMenuVisible` is written by the menu export; binding
// `visible: true` here puts the menu in two places at once.
Tk.MenuBar {
    id: bar

    required property var menusModel
    signal activated(string actionId)

    Instantiator {
        model: bar.menusModel
        delegate: Tk.Menu {
            id: menu
            required property var modelData
            title: menu.modelData.label

            Instantiator {
                model: menu.modelData.actions
                delegate: Tk.MenuItem {
                    required property var modelData
                    text: modelData.label
                    shortcut: modelData.shortcut
                    enabled: modelData.enabled
                    checkable: modelData.checkable
                    checked: modelData.checked
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
