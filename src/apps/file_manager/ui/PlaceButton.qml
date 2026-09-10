// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Sidebar button with a themed leading icon and an elided label. "emphasized"
// marks the currently open folder with the palette's alternate base.
ToolButton {
    id: control
    required property string iconName
    property bool emphasized: false
    padding: 8
    flat: true
    implicitWidth: 120
    implicitHeight: 44
    display: AbstractButton.TextBesideIcon
    background: Rectangle {
        radius: 6
        color: control.down ? control.palette.mid
             : control.hovered ? control.palette.alternateBase
             : control.emphasized ? control.palette.alternateBase
             : "transparent"
        border.width: control.activeFocus ? 2 : 0
        border.color: control.palette.highlight
    }
    contentItem: RowLayout {
        spacing: 8
        Image {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            source: "image://theme-icons/" + control.iconName
            sourceSize: Qt.size(24, 24)
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            text: control.text
            elide: Text.ElideRight
            Accessible.ignored: true
        }
    }
}
