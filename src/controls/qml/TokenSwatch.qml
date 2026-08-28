// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

T.Control {
    id: control

    property color value: Tokens.accent.default
    property string label: ""
    property string accessibleDescription: ""

    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: Math.max(32, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: Tokens.space["2"]
    rightPadding: Tokens.space["2"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]
    Accessible.role: Accessible.StaticText
    Accessible.name: label
    Accessible.description: accessibleDescription

    contentItem: RowLayout {
        spacing: Tokens.space["2"]

        Rectangle {
            implicitWidth: 24
            implicitHeight: 24
            radius: Tokens.radius.s
            color: control.value
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.strong
            Accessible.ignored: true
        }

        Text {
            visible: control.label.length > 0
            text: control.label
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            Accessible.ignored: true
        }
    }
}
