// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// A console desk pad for the tray (M, and whatever joins it): the smallest
// clickable unit the console has. The shared form Button carries a 96x40
// implicit floor for pointer comfort, which is correct in a settings form and
// wrong on a console row — one strip's mute rendered as a 96 px accent block
// beside a 3 px meter, which is what the before-capture shows.
//
// AGENT-NOTE: the deliberate twin of
// src/apps/settings/audio/qml/AudioConsolePad.qml. The two QML modules cannot
// import each other and src/controls is off-limits to this lane, so the desk
// idiom is spelled twice on purpose; keep them in step.
T.Button {
    id: pad

    property bool available: true
    property bool destructive: false
    property color lampColor: Tokens.accent.default
    property color lampTextColor: Tokens.accent.fg
    property string accessibleDescription: ""

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    enabled: available
    implicitWidth: Math.max(24, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 22
    leftPadding: Tokens.space["1"]
    rightPadding: Tokens.space["1"]
    topPadding: 1
    bottomPadding: 1
    font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: accessibleDescription
    Accessible.checkable: checkable
    Accessible.checked: checked

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

        C.FocusRing {
            anchors.fill: parent
            control: pad
        }
    }
}
