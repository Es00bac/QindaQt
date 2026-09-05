// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// Compact virtual-desktop switcher (MATE, XFCE, Unity, minimal, QindaQt
// presets): numbered buttons, current workspace highlighted, arrow keys move
// between buttons, Space/Return switch through the workspace facade.
Item {
    id: root

    required property var access
    property bool vertical: false

    objectName: "workspaceSwitcherApplet"
    implicitWidth: strip.implicitWidth + Tokens.space["2"]
    implicitHeight: vertical ? strip.implicitHeight + Tokens.space["2"] : 28

    WorkspaceStrip {
        id: strip
        objectName: "workspaceSwitcherStrip"
        anchors.fill: parent
        anchors.margins: Tokens.space["1"] / 2
        access: root.access
        vertical: root.vertical
        tiles: false
    }
}
