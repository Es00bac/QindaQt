// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.Slider {
    id: control

    property string accessibleName: ""
    property string accessibleDescription: ""
    readonly property int transitionDuration: Tokens.motion.short
    readonly property real effectiveProgress: mirrored ? 1.0 - visualPosition
                                                       : visualPosition

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    implicitWidth: 220
    implicitHeight: 36

    Accessible.role: Accessible.Slider
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription

    background: Item {
        objectName: "sliderTrack"
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 6

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Tokens.bg.highest
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.divider
        }

        Rectangle {
            objectName: "progressFill"
            x: control.mirrored ? parent.width - width : 0
            width: control.effectiveProgress * parent.width
            height: parent.height
            radius: height / 2
            color: control.enabled ? Tokens.accent.default : Tokens.fg.disabled

            Behavior on width {
                NumberAnimation { duration: control.transitionDuration }
            }
        }
    }

    handle: Rectangle {
        objectName: "sliderHandle"
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 22
        implicitHeight: 22
        radius: height / 2
        color: control.enabled ? Tokens.bg.highest : Tokens.fg.disabled
        border.width: control.activeFocus ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: control.activeFocus ? Tokens.focus.ring : Tokens.outline.strong

        Behavior on x {
            NumberAnimation { duration: control.transitionDuration }
        }
    }
}
