// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Tokens 1.0

Rectangle {
    id: card
    required property string title
    required property string body
    property string marker: ""

    implicitHeight: content.implicitHeight + Tokens.space["5"] * 2
    radius: Tokens.radius.l
    color: Tokens.bg.raised
    border.width: 1
    border.color: Tokens.outline.divider
    Accessible.role: Accessible.StaticText
    Accessible.name: title
    Accessible.description: body

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["4"]

        Rectangle {
            visible: card.marker.length > 0
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignTop
            radius: Tokens.radius.m
            color: Tokens.accent.subtle
            border.width: 1
            border.color: Tokens.accent.default
            Text {
                anchors.centerIn: parent
                text: card.marker
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: Font.DemiBold
                Accessible.ignored: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]
            Text {
                Layout.fillWidth: true
                text: card.title
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }
            Text {
                Layout.fillWidth: true
                text: card.body
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                wrapMode: Text.Wrap
                lineHeight: 1.18
                Accessible.ignored: true
            }
        }
    }
}
