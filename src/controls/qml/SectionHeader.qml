// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

T.Control {
    id: control

    property string title: ""
    property string description: ""

    leftPadding: 0
    rightPadding: 0
    topPadding: Tokens.space["3"]
    bottomPadding: Tokens.space["3"]
    Accessible.role: Accessible.StaticText
    Accessible.name: title
    Accessible.description: description

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        Text {
            Layout.fillWidth: true
            text: control.title
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            Accessible.ignored: true
        }

        Text {
            Layout.fillWidth: true
            visible: control.description.length > 0
            text: control.description
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            wrapMode: Text.Wrap
            Accessible.ignored: true
        }
    }
}
