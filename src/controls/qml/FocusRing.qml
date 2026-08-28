// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

Rectangle {
    id: ring

    required property Item control
    readonly property bool focusVisible: control.enabled && control.activeFocus

    visible: focusVisible
    color: "transparent"
    radius: Tokens.radius.m
    border.width: Tokens.space["1"]
    border.color: Tokens.focus.ring
    Accessible.ignored: true
}
