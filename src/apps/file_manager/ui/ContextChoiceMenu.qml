// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// A sub-menu of exclusive choices -- Sort By and View in FileContextMenu
// (ADR-0269). Each choice runs its catalog command and the check follows
// `current`. The group keeps a chosen entry checked, so choosing the current
// one again never leaves a stale unchecked state (Sort By then reverses the
// order, as a column header does).
Menu {
    id: menu

    required property var contextMenu
    // [{name, text, actionId, value}]; `name` is the entry's objectName.
    required property var choices
    // A var, not a string: a minimal fixture navigation may lack the property.
    property var current: ""

    ButtonGroup { id: group }

    Instantiator {
        model: menu.choices
        delegate: MenuItem {
            required property var modelData
            objectName: modelData.name
            text: modelData.text
            checkable: true
            ButtonGroup.group: group
            checked: menu.current === modelData.value
            enabled: menu.contextMenu.actionEnabled(modelData.actionId)
            onTriggered: menu.contextMenu.appCoordinator.activateAction(modelData.actionId)
        }
        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}
