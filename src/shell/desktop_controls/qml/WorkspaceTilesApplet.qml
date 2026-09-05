// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// NeXTSTEP-style workspace tiles: the same workspace facade as the compact
// switcher, presented as named tiles sized for a wide dock.
Item {
    id: root

    required property var access
    property bool vertical: false

    objectName: "workspaceTilesApplet"
    implicitWidth: strip.implicitWidth + Tokens.space["2"]
    implicitHeight: strip.implicitHeight + Tokens.space["2"]

    WorkspaceStrip {
        id: strip
        objectName: "workspaceTilesStrip"
        anchors.fill: parent
        anchors.margins: Tokens.space["1"] / 2
        access: root.access
        vertical: root.vertical
        tiles: true
    }
}
