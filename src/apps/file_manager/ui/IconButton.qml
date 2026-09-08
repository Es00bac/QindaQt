// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

Qinda.Button {
    id: control
    required property string iconName
    implicitWidth: 40
    implicitHeight: 40
    leftPadding: Tokens.space["2"]
    rightPadding: Tokens.space["2"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]
    emphasized: false
    background: Rectangle {
        radius: Tokens.radius.m
        color: control.down ? Tokens.state.pressed : control.hovered ? Tokens.state.hover
             : control.emphasized ? Tokens.state.pressed : "transparent"
        border.width: control.activeFocus ? 2 : 0
        border.color: Tokens.focus.ring
    }
    contentItem: Qinda.Icon {
        name: control.iconName + "-symbolic"
        color: control.enabled ? Tokens.fg.default : Tokens.fg.muted
        implicitWidth: 20
        implicitHeight: 20
    }
    T.ToolTip.visible: hovered || activeFocus
    T.ToolTip.delay: 600
    T.ToolTip.text: text
}
