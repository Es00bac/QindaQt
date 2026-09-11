// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Square icon button for the Audio route, following the Customize route's
// icon-first style: glyph plus tooltip instead of a labeled row. Sizing keeps
// the documented panel hit-target floor so nothing becomes unclickable in a
// narrow column.
T.AbstractButton {
    id: control

    property string iconName: ""
    property string toolTip: ""
    property int iconSize: 18
    property bool available: true
    property bool destructive: false
    property bool emphasized: false

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    enabled: control.available
    implicitWidth: 40
    implicitHeight: 40
    padding: 0
    spacing: 0

    Accessible.role: Accessible.Button
    Accessible.name: control.toolTip.length > 0 ? control.toolTip : control.iconName
    Accessible.description: control.toolTip
    T.ToolTip.visible: control.hovered && control.toolTip.length > 0
    T.ToolTip.delay: 500
    T.ToolTip.text: control.toolTip

    icon.name: ""
    icon.source: ""
    text: ""

    contentItem: Item {
        ShellIcons.Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            symbolic: true
            // AGENT-GUARD: the icon recolor must be fully opaque — a
            // translucent token is "no recolor" to the provider and the glyph
            // vanishes on dark themes. Disabled dims via opacity instead.
            color: !control.enabled ? Tokens.fg.muted
                  : control.destructive ? Tokens.danger.fg
                  : control.emphasized ? Tokens.accent.fg : Tokens.fg.default
            opacity: control.enabled ? 1.0 : 0.6
            fallbackText: control.toolTip
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: !control.enabled ? Tokens.bg.base
             : control.destructive && control.down ? Tokens.danger.default
             : control.emphasized ? Tokens.accent.default : Tokens.bg.raised
        border.width: Tokens.space["1"] / 2
        border.color: control.destructive ? Tokens.danger.default
                     : control.emphasized ? Tokens.outline.strong
                     : Tokens.outline.divider

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: !control.enabled ? "transparent"
                 : control.down ? Tokens.state.pressed
                 : control.hovered ? Tokens.state.hover : "transparent"
            Accessible.ignored: true
        }

        FocusRing {
            anchors.fill: parent
            control: control
        }
    }
}
