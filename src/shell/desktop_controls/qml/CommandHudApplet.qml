// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// Unity-style HUD: searches only the active application's exported menu
// actions through the global menu facade and activates them there.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready

    objectName: "commandHudApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    DesktopControlButton {
        id: button
        objectName: "commandHudButton"
        anchors.fill: parent
        iconName: "edit-find"
        fallbackText: qsTr("HUD")
        labelText: qsTr("HUD")
        showLabel: true
        vertical: root.vertical
        available: root.ready && Boolean(root.access.available)
        active: hud.opened
        Accessible.name: qsTr("Menu search")
        accessibleDescription: available
                               ? qsTr("Search the active application's menu actions")
                               : qsTr("No application menu is available to search")
        onTriggered: if (available) hud.open()
    }

    CommandSearchPopup {
        id: hud
        objectName: "commandHudPopup"
        access: root.access
        heading: qsTr("Menu search")
        placeholderText: qsTr("Type a menu action")
        emptyText: qsTr("No menu actions match")
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)
    }
}
