// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

// One key of the on-screen keyboard. A finger or a mouse presses it; the
// model decides what the press means.
Rectangle {
    id: cap

    required property int rowIndex
    required property int keyIndex
    required property string kind
    required property string label
    required property bool active

    signal pressedKey(int rowIndex, int keyIndex)

    readonly property bool control: kind !== "text" && kind !== "space"

    objectName: "oskKey_" + rowIndex + "_" + keyIndex
    radius: Tokens.radius.m
    color: tap.pressed ? Tokens.accent.default
         : active ? Tokens.accent.default
         : control ? Tokens.bg.highest
         : Tokens.bg.base
    border.width: 1
    border.color: Tokens.outline.strong

    Text {
        anchors.centerIn: parent
        text: cap.label
        color: (tap.pressed || cap.active) ? Tokens.accent.fg : Tokens.fg.default
        font.family: Tokens.type.fontFamily
        font.pointSize: cap.control ? Tokens.type.body : Tokens.type.title
    }

    TapHandler {
        id: tap
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: cap.pressedKey(cap.rowIndex, cap.keyIndex)
    }
}
