// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// One catalog command in FileContextMenu (ADR-0269): enabled exactly when the
// AppShell catalog says so, and triggered through the same coordinator path
// the menu bar uses. Visibility stays with the menu's own rules.
MenuItem {
    required property var contextMenu
    required property string actionId

    enabled: contextMenu.actionEnabled(actionId)
    onTriggered: contextMenu.appCoordinator.activateAction(actionId)
}
