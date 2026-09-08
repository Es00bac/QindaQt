// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as Qinda
import QindaQt.Tokens 1.0
Qinda.Button {
    id: control
    required property string iconName
    leftPadding: Tokens.space["2"]
    rightPadding: Tokens.space["2"]
    implicitWidth: 120
    implicitHeight: 44
    background: Rectangle {
        radius: Tokens.radius.m
        color: control.down ? Tokens.state.pressed : control.hovered ? Tokens.state.hover
             : control.emphasized ? Tokens.state.pressed : "transparent"
        border.width: control.activeFocus ? 2 : 0
        border.color: Tokens.focus.ring
    }
    contentItem: RowLayout {
        spacing: Tokens.space["2"]
        Qinda.Icon {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            name: control.iconName
        }
        Qinda.Label {
            Layout.fillWidth: true
            text: control.text
            elide: Text.ElideRight
            Accessible.ignored: true
        }
    }
}
