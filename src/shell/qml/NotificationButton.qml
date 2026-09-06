// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T

T.Button {
    id: control
    required property var theme
    readonly property var colors: theme.colors ?? ({})
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? (control.colors.text ?? "#f2f1eb")
                               : (control.colors.textMuted ?? "#a9afa9")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }
    background: Rectangle {
        radius: Math.min(control.theme.cornerRadius ?? 10, 8)
        color: control.down ? (control.colors.surface ?? "#222624")
                            : (control.colors.surfaceRaised ?? "#2c312e")
        border.width: 1
        border.color: control.activeFocus ? (control.colors.accent ?? "#8fc8b7")
                                          : (control.colors.border ?? "#3c433f")
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.colors.accent ?? "#8fc8b7"
            opacity: control.hovered && control.enabled ? 0.10 : 0
        }
    }
}
