// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// A console desk pad (M, S, Mono, A1, B2, Rack, Record…): the smallest
// clickable unit the mixing console has. The shared form Button carries a
// 96x40 implicit floor for pointer comfort, which is correct everywhere
// except the console, where sixteen pads per strip would make the routing
// matrix wider than the window. Here the text IS the button: caption font,
// ~2px of padding, checked state lit like a desk lamp.
// AGENT-CONTRACT: the probe scripts find these by the same objectNames the
// old full-size buttons carried (consoleStripMute_, consoleSend_…), and the
// page test drives them through the same clicked/toggled signals.
T.Button {
    id: pad

    property bool available: true
    property bool busy: false
    property bool destructive: false
    // The lit colours. VoiceMeeter parity: an engaged routing key glows in
    // the accent colour; mute and record lamps read as danger instead.
    property color lampColor: Tokens.accent.default
    property color lampTextColor: Tokens.accent.fg

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    enabled: available && !busy
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: 20
    leftPadding: Tokens.space["1"]
    rightPadding: Tokens.space["1"]
    topPadding: 1
    bottomPadding: 1
    font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })

    Accessible.role: Accessible.Button
    Accessible.name: text

    contentItem: Text {
        text: pad.text
        color: !pad.enabled ? Tokens.fg.muted
             : pad.checked ? pad.lampTextColor : Tokens.fg.default
        font: pad.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Tokens.radius.s
        color: !pad.enabled ? Tokens.bg.raised
             : pad.checked ? pad.lampColor : Tokens.bg.raised
        border.width: Tokens.space["1"] / 2
        border.color: pad.destructive ? Tokens.danger.default
                     : Tokens.outline.strong

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: !pad.enabled ? "transparent"
                 : pad.down ? Tokens.state.pressed
                 : pad.hovered ? Tokens.state.hover : "transparent"
            Accessible.ignored: true
        }

        FocusRing {
            anchors.fill: parent
            control: pad
        }
    }
}
