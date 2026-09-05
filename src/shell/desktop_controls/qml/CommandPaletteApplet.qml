// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// Keyboard-first command palette (minimal preset): one search over
// applications, the active window's menu actions, windows, and workspaces.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready

    objectName: "commandPaletteApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    DesktopControlButton {
        id: button
        objectName: "commandPaletteButton"
        anchors.fill: parent
        iconName: "system-search"
        fallbackText: qsTr("Commands")
        labelText: qsTr("Commands")
        showLabel: true
        vertical: root.vertical
        available: root.ready && Boolean(root.access.available)
        active: palette.opened
        Accessible.name: qsTr("Command palette")
        accessibleDescription: available
                               ? qsTr("Search applications, menu actions, windows, and workspaces")
                               : qsTr("The command palette is unavailable")
        onTriggered: if (available) palette.open()
    }

    CommandSearchPopup {
        id: palette
        objectName: "commandPalettePopup"
        access: root.access
        heading: qsTr("Command palette")
        placeholderText: qsTr("Type a command, application, or window")
        emptyText: qsTr("No commands match")
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)
    }
}
