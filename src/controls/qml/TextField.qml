// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

T.TextField {
    id: control

    property string accessibleName: ""
    property string accessibleDescription: ""
    property bool error: false
    readonly property int transitionDuration: Tokens.motion.short
    readonly property string effectiveAccessibleDescription: error
        ? (accessibleDescription.length > 0
           ? qsTr("Error. %1").arg(accessibleDescription) : qsTr("Error"))
        : accessibleDescription

    focusPolicy: Qt.StrongFocus
    selectByMouse: true
    implicitHeight: Math.max(40,
                             control.contentHeight
                             + topPadding + bottomPadding)
    leftPadding: Tokens.space["4"]
    rightPadding: Tokens.space["4"]
    topPadding: Tokens.space["3"]
    bottomPadding: Tokens.space["3"]
    color: enabled ? Tokens.fg.default : Tokens.fg.disabled
    placeholderTextColor: Tokens.fg.muted
    selectionColor: Tokens.accent.default
    selectedTextColor: Tokens.accent.fg
    font.family: Tokens.type.fontFamily
    font.pointSize: Tokens.type.body

    Accessible.role: Accessible.EditableText
    Accessible.name: accessibleName.length > 0 ? accessibleName : placeholderText
    Accessible.description: effectiveAccessibleDescription

    background: Rectangle {
        radius: Tokens.radius.m
        color: Tokens.bg.highest
        border.width: control.activeFocus ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: control.error ? Tokens.danger.default
                     : control.activeFocus ? Tokens.focus.ring : Tokens.outline.strong

        Behavior on border.color {
            ColorAnimation { duration: control.transitionDuration }
        }
    }
}
