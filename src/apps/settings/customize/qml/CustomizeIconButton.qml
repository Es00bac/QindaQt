// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Square icon button for the Customize route. Icon + tooltip instead of
// labeled text rows: the route's controls are recognized by glyph and
// explained on hover, keeping the work area visual. Sizing follows the
// documented panel hit-target floor (never below 24px of hit area; 40px
// here) so nothing in this route can become unclickable in a narrow column.
T.AbstractButton {
    id: control

    property string iconName: ""
    // Optional drawn glyph ("align-start", "align-center", "align-end",
    // "align-fill", "zone-start", "zone-center", "zone-end") used where no
    // XDG icon expresses the concept. When set it replaces the icon.
    property string glyphName: ""
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
            visible: control.glyphName.length === 0
            name: control.iconName
            size: control.iconSize
            symbolic: true
            // AGENT-GUARD: the icon recolor must be fully opaque — a
            // translucent token (like the disabled foreground overlay) is
            // "no recolor" to the provider and the glyph vanishes on dark
            // themes. Disabled dims via opacity on an opaque muted color.
            color: !control.enabled ? Tokens.fg.muted
                  : control.destructive ? Tokens.danger.fg
                  : control.emphasized ? Tokens.accent.fg : Tokens.fg.default
            opacity: control.enabled ? 1.0 : 0.6
            fallbackText: control.toolTip
        }

        // Drawn glyphs for concepts without an XDG icon. Pure token shapes so
        // they render identically in every environment, tests included.
        Item {
            id: glyph

            readonly property color glyphColor: !control.enabled ? Tokens.fg.disabled
                    : control.emphasized ? Tokens.accent.fg : Tokens.fg.default

            visible: control.glyphName.length > 0
            width: 18
            height: 12
            anchors.centerIn: parent

            Repeater {
                model: control.glyphName === "align-fill"
                       ? [{ w: 18, x: 0 }]
                       : [{ w: 16, x: 0 }, { w: 12, x: 0 }, { w: 8, x: 0 }]

                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    width: modelData.w
                    height: 2
                    radius: 1
                    color: glyph.glyphColor
                    x: control.glyphName === "align-center"
                       ? (glyph.width - width) / 2
                       : control.glyphName === "align-end"
                         ? glyph.width - width : modelData.x
                    y: index * 5
                }
            }

            Rectangle {
                visible: ["zone-start", "zone-center", "zone-end"]
                         .includes(control.glyphName)
                width: 18
                height: 4
                radius: 2
                color: "transparent"
                border.width: 1
                border.color: glyph.glyphColor
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 4
                    height: 4
                    radius: 2
                    color: glyph.glyphColor
                    anchors.verticalCenter: parent.verticalCenter
                    x: control.glyphName === "zone-start" ? 1
                       : control.glyphName === "zone-end" ? parent.width - 5
                       : (parent.width - width) / 2
                }
            }
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: !control.enabled ? Tokens.bg.base
             : control.destructive && control.down ? Tokens.danger.default
             : control.emphasized ? Tokens.accent.default : Tokens.bg.raised
        border.width: control.emphasized ? Tokens.space["1"] / 2 : Tokens.space["1"] / 2
        border.color: control.destructive ? Tokens.danger.default
                     : control.emphasized ? Tokens.outline.strong : Tokens.outline.divider

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
