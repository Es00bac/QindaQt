// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.Switch {
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
    implicitHeight: Math.max(36, implicitContentHeight + topPadding + bottomPadding)

    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.description: accessibleDescription
    Accessible.checkable: true
    Accessible.checked: checked

    indicator: Rectangle {
        objectName: "switchTrack"
        implicitWidth: 44
        implicitHeight: 24
        x: control.mirrored ? control.width - width - control.rightPadding
                            : control.leftPadding
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? Tokens.accent.default : Tokens.bg.highest
        border.width: Tokens.space["1"] / 2
        border.color: control.activeFocus ? Tokens.focus.ring : Tokens.outline.strong

        Rectangle {
            objectName: "switchKnob"
            width: 18
            height: 18
            radius: height / 2
            y: (parent.height - height) / 2
            x: 3 + control.visualPosition * (parent.width - width - 6)
            color: control.checked ? Tokens.accent.fg : Tokens.fg.muted
            Accessible.ignored: true

            Behavior on x {
                NumberAnimation { duration: control.transitionDuration }
            }
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
