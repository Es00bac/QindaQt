// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Rectangle {
    id: root

    required property string title
    required property string detail
    required property string glyph
    required property var theme
    property bool terminal: false
    readonly property var colors: theme.colors ?? ({})

    color: terminal ? Qt.darker(colors.canvas ?? "#171a18", 1.08) : colors.surface ?? "#222624"
    border.color: colors.border ?? "#3c433f"

    Rectangle {
        id: memberHeader
        width: parent.width
        height: 30
        color: root.colors.surfaceRaised ?? "#2c312e"
        border.color: root.colors.border ?? "#3c433f"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: root.colors.text ?? "white"
            font.pixelSize: 11
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            text: "⋮  ×"
            color: root.colors.textMuted ?? "#a9afa9"
        }
    }

    Text {
        anchors.fill: parent
        anchors.topMargin: memberHeader.height + 22
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        text: root.glyph + "\n\n" + root.detail
        color: root.colors.text ?? "white"
        font.family: root.terminal ? root.theme.monoFontFamily : root.theme.fontFamily
        font.pixelSize: root.terminal ? 13 : 16
        wrapMode: Text.Wrap
    }
}
