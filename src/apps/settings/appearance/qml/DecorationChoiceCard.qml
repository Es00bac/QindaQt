// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Tokens 1.0

// A decoration catalog choice deliberately does not invent a thumbnail.
// Foreign KDecoration/Aurorae plugins own their rendering; once applied, the
// Settings window's own frame is the authoritative live preview.
T.AbstractButton {
    id: control

    required property string decorationName
    required property string decorationKind
    property bool available: true

    checkable: true
    autoExclusive: true
    enabled: available
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: Tokens.space["3"]
    implicitWidth: 210
    implicitHeight: 68
    Accessible.role: Accessible.RadioButton
    Accessible.name: decorationName
    Accessible.description: decorationKind === "aurorae"
                            ? qsTr("Installed Aurorae window decoration")
                            : qsTr("Installed native window decoration")
    Accessible.checkable: true
    Accessible.checked: checked

    contentItem: RowLayout {
        spacing: Tokens.space["3"]

        Rectangle {
            Layout.preferredWidth: 42
            Layout.preferredHeight: 34
            radius: Tokens.radius.s
            color: Tokens.bg.base
            border.width: Tokens.space["1"] / 2
            border.color: Tokens.outline.strong

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 11
                radius: parent.radius
                color: Tokens.bg.highest
            }

            Row {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 3
                spacing: 2
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        width: 5
                        height: 5
                        radius: 3
                        color: index === 0 ? Tokens.danger.default
                             : index === 1 ? Tokens.accent.default
                                           : Tokens.fg.muted
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: control.decorationName
            color: control.enabled ? Tokens.fg.default : Tokens.fg.disabled
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            font.weight: control.checked ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
        }

        Text {
            visible: control.checked
            text: "✓"
            color: Tokens.fg.default
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.subtitle
            font.bold: true
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: control.checked ? Tokens.accent.subtle
                               : control.hovered ? Tokens.state.hover
                                                 : Tokens.bg.raised
        border.width: control.activeFocus || control.checked
                      ? Tokens.space["1"] : Tokens.space["1"] / 2
        border.color: control.activeFocus ? Tokens.focus.ring
                     : control.checked ? Tokens.accent.default
                                       : Tokens.outline.divider
    }
}
