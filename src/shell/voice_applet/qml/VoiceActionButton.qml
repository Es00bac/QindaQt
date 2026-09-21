// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// One projected voice action, rendered as a compact icon-and-label button.
//
// AGENT-CONTRACT: `enabled` comes from the projection, never from local
// reasoning about state. A button this file enables on its own would be a
// button the controller refuses, which reads to the user as a dead control.
T.ToolButton {
    id: control

    required property string actionId
    required property string iconName
    required property bool actionEnabled
    property bool emphasized: false

    signal activated(string actionId)

    enabled: actionEnabled
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    implicitWidth: Math.max(64, row.implicitWidth + Tokens.space["3"] * 2)
    implicitHeight: 30

    Accessible.role: Accessible.Button
    Accessible.name: text

    onClicked: control.activated(control.actionId)
    Accessible.onPressAction: if (enabled) control.activated(control.actionId)

    contentItem: Row {
        id: row
        anchors.centerIn: parent
        spacing: Tokens.space["1"]

        ShellIcons.Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: 14
            color: !control.enabled ? Tokens.fg.disabled
                 : control.emphasized ? Tokens.accent.fg : Tokens.fg.default
            symbolic: true
            fallbackText: ""
            Accessible.ignored: true
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: !control.enabled ? Tokens.fg.disabled
                 : control.emphasized ? Tokens.accent.fg : Tokens.fg.default
            font.family: Tokens.type.fontFamily
            font.pixelSize: Tokens.type.caption
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: Tokens.radius.s
        color: !control.enabled ? "transparent"
             : control.emphasized ? Tokens.accent.default
             : control.pressed ? Tokens.state.pressed
             : control.hovered ? Tokens.state.hover
             : "transparent"
        border.color: control.enabled && !control.emphasized ? Tokens.outline.divider
                                                             : "transparent"

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            visible: control.visualFocus
            border.color: Tokens.focus.ring
            border.width: 2
        }
    }
}
