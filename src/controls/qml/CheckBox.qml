// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.CheckBox {
    id: control

    property string accessibleDescription: ""
    readonly property int transitionDuration: Tokens.motion.short

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    spacing: Tokens.space["3"]
    leftPadding: Tokens.space["2"]
    rightPadding: Tokens.space["2"]
    topPadding: Tokens.space["2"]
    bottomPadding: Tokens.space["2"]
    implicitHeight: Math.max(32, implicitContentHeight + topPadding + bottomPadding)

    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.description: accessibleDescription
    Accessible.checkable: true
    Accessible.checked: checked

    indicator: Rectangle {
        implicitWidth: 22
        implicitHeight: 22
        x: control.mirrored ? control.width - width - control.rightPadding
                            : control.leftPadding
        y: (control.height - height) / 2
        radius: Tokens.radius.s
        color: control.checked ? Tokens.accent.default : Tokens.bg.highest
        border.width: Tokens.space["1"] / 2
        border.color: control.activeFocus ? Tokens.focus.ring : Tokens.outline.strong

        Text {
            anchors.centerIn: parent
            text: control.checked ? "✓" : ""
            color: Tokens.accent.fg
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            font.bold: true
            Accessible.ignored: true
        }

        Behavior on color {
            ColorAnimation { duration: control.transitionDuration }
        }
    }

    contentItem: Text {
        leftPadding: control.mirrored ? 0 : control.indicator.width + control.spacing
        rightPadding: control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
        font.family: Tokens.type.fontFamily
        font.pointSize: Tokens.type.body
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: control.mirrored ? Text.AlignRight : Text.AlignLeft
        wrapMode: Text.Wrap
    }
}
