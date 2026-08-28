// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.Button {
    id: control

    property bool busy: false
    property bool available: true
    property bool destructive: false
    property bool emphasized: true
    property bool error: false
    property string accessibleDescription: ""
    readonly property string effectiveAccessibleDescription: error
        ? (accessibleDescription.length > 0
           ? qsTr("Error. %1").arg(accessibleDescription) : qsTr("Error"))
        : accessibleDescription
    readonly property int transitionDuration: Tokens.motion.short

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    // AGENT-CONTRACT: Consumers express capability through available. Directly
    // overriding inherited enabled replaces this QML binding and is unsupported.
    enabled: available && !busy
    implicitWidth: Math.max(96, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(40, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: Tokens.space["4"]
    rightPadding: Tokens.space["4"]
    topPadding: Tokens.space["3"]
    bottomPadding: Tokens.space["3"]

    Accessible.role: Accessible.Button
    Accessible.name: busy ? qsTr("%1, busy").arg(text) : text
    Accessible.description: effectiveAccessibleDescription

    contentItem: Text {
        text: control.busy ? qsTr("Working…") : control.text
        color: !control.enabled ? Tokens.fg.disabled
              : control.destructive ? Tokens.danger.fg
              : control.emphasized ? Tokens.accent.fg : Tokens.fg.default
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.body
        font.weight: control.emphasized ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: control.destructive ? Tokens.danger.default
             : control.emphasized ? Tokens.accent.default : Tokens.bg.raised
        border.width: Tokens.space["1"] / 2
        border.color: control.error ? Tokens.danger.default : Tokens.outline.strong

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Tokens.state.pressed
                 : control.hovered ? Tokens.state.hover : "transparent"
            Accessible.ignored: true

            Behavior on color {
                ColorAnimation { duration: control.transitionDuration }
            }
        }

        FocusRing {
            objectName: "focusRing"
            anchors.fill: parent
            control: control
        }
    }
}
