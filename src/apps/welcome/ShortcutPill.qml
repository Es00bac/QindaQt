// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

Rectangle {
    id: pill
    required property string text
    implicitWidth: label.implicitWidth + Tokens.space["3"] * 2
    implicitHeight: label.implicitHeight + Tokens.space["2"] * 2
    radius: Tokens.radius.s
    color: Tokens.bg.highest
    border.width: 1
    border.color: Tokens.outline.strong
    Accessible.role: Accessible.StaticText
    Accessible.name: text
    Text {
        id: label
        anchors.centerIn: parent
        text: pill.text
        color: Tokens.fg.default
        font.family: Tokens.type.monoFontFamily
        font.pointSize: Tokens.type.caption
        font.weight: Font.DemiBold
        Accessible.ignored: true
    }
}
