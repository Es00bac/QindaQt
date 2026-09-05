// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// One keyboard-focusable menu or result row: icon, text, optional detail.
// Space (inherited), Return, and Enter activate; the caller owns the intent.
T.Button {
    id: row

    property string iconName: ""
    property string detail: ""
    property bool destructive: false
    property bool current: false
    property string accessibleDescription: detail

    signal activated()

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    padding: Tokens.space["2"]
    implicitWidth: Math.max(220, contentRow.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(32, contentRow.implicitHeight + topPadding + bottomPadding)

    Accessible.role: Accessible.MenuItem
    Accessible.name: text
    Accessible.description: accessibleDescription

    onClicked: row.activated()
    Keys.onReturnPressed: if (enabled) row.activated()
    Keys.onEnterPressed: if (enabled) row.activated()
    Accessible.onPressAction: if (enabled) row.activated()

    contentItem: RowLayout {
        id: contentRow
        spacing: Tokens.space["2"]

        ShellIcons.Icon {
            objectName: "menuRowIcon"
            visible: row.iconName.length > 0 || row.text.length > 0
            name: row.iconName
            size: 18
            color: row.enabled ? Tokens.fg.default : Tokens.fg.disabled
            symbolic: true
            fallbackText: row.text
            Accessible.ignored: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                objectName: "menuRowText"
                Layout.fillWidth: true
                text: row.text
                color: !row.enabled ? Tokens.fg.disabled
                     : row.destructive ? Tokens.danger.default : Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: row.current ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                Accessible.ignored: true
            }

            Text {
                objectName: "menuRowDetail"
                Layout.fillWidth: true
                visible: row.detail.length > 0
                text: row.detail
                color: row.enabled ? Tokens.fg.muted : Tokens.fg.disabled
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                elide: Text.ElideRight
                Accessible.ignored: true
            }
        }
    }

    background: Rectangle {
        radius: Tokens.radius.m
        color: row.down ? Tokens.state.pressed
             : row.hovered || row.activeFocus ? Tokens.state.hover
             : "transparent"

        C.FocusRing {
            anchors.fill: parent
            control: row
        }
    }
}
