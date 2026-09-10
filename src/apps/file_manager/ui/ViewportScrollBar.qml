// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// AGENT-GUARD: Keep the gutter reserved even when content fits. Toggling it
// with overflow can repeatedly reflow the grid across its overflow threshold.
ScrollBar {
    id: control
    width: 12
    orientation: Qt.Vertical
    policy: ScrollBar.AlwaysOn
    visible: size < 1
    minimumSize: height > 0 ? Math.min(1, 28 / height) : 1
    hoverEnabled: true
    padding: 2
    Accessible.name: qsTr("Scroll folder contents")
    contentItem: Rectangle {
        implicitWidth: 8
        radius: width / 2
        color: control.pressed ? control.palette.highlight
             : control.hovered ? control.palette.text : control.palette.placeholderText
    }
    background: Rectangle {
        radius: width / 2
        color: control.hovered || control.pressed ? control.palette.alternateBase : "transparent"
    }
}
