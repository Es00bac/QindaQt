// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic as Basic
import QindaQt.Tokens 1.0

Basic.MenuItem {
    id: control
    required property var entryData
    required property var access
    required property var targetItem
    required property string revision
    property bool interactive: true
    objectName: entryData.kind === "submenu" ? "statusNotifierSubmenuItem" : "statusNotifierMenuAction"
    text: String(entryData.label ?? "")
    enabled: interactive && entryData.enabled !== false
    implicitWidth: 292
    implicitHeight: Math.max(30, Tokens.type.body + 14)
    // The application is authoritative for toggles; wait for its menu update.
    checkable: false
    checked: Boolean(entryData.checked)
    Accessible.role: Accessible.MenuItem
    Accessible.name: text
    Accessible.checkable: Boolean(entryData.checkable)
    Accessible.checked: Boolean(entryData.checked)
    onTriggered: {
        if (entryData.kind === "action" && enabled && targetItem)
            access.invokeMenu(targetItem.uniqueName, targetItem.objectPath,
                              targetItem.generation, revision, Number(entryData.id))
    }
    background: Rectangle {
        radius: Tokens.radius.s
        color: control.highlighted ? Tokens.state.hover : "transparent"
    }
    contentItem: Row {
        spacing: 8
        Text {
            width: 18
            text: Boolean(control.entryData.checked)
                ? (Boolean(control.entryData.radio) ? "●" : "✓") : ""
            color: control.enabled ? Tokens.fg.default : Tokens.fg.muted
            font.family: Tokens.type.fontFamily
        }
        Text {
            width: Math.max(0, control.availableWidth - 44)
            text: control.text
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.body
            color: control.enabled ? Tokens.fg.default : Tokens.fg.muted
        }
        Text {
            width: 10
            text: control.entryData.kind === "submenu" ? "›" : ""
            color: Tokens.fg.muted
        }
    }
}
