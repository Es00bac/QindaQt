// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

// AGENT-GUARD: Keep the gutter reserved even when content fits. Toggling it
// with overflow can repeatedly reflow the grid across its overflow threshold.
T.ScrollBar {
    id: control
    width: 12
    orientation: Qt.Vertical
    policy: T.ScrollBar.AlwaysOn
    visible: size < 1
    minimumSize: height > 0 ? Math.min(1, 28 / height) : 1
    hoverEnabled: true
    padding: 2
    Accessible.name: qsTr("Scroll folder contents")
    contentItem: Rectangle {
        implicitWidth: 8
        radius: width / 2
        color: control.pressed ? Tokens.accent.default
             : control.hovered ? Tokens.fg.default : Tokens.fg.muted
    }
    background: Rectangle {
        radius: width / 2
        color: control.hovered || control.pressed ? Tokens.state.hover : "transparent"
    }
}
