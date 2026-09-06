// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic

Basic.MenuItem {
    id: control

    required property var entryData
    required property var access
    required property var colors
    property bool interactive: true

    objectName: String(entryData.kind ?? "action") === "submenu"
        ? "globalMenuNativeSubmenuItem" : "globalMenuNativeAction"
    text: String(entryData.text ?? "")
    enabled: interactive && Boolean(entryData.enabled)
        && (String(entryData.kind ?? "action") !== "submenu"
            || (entryData.children ?? []).length > 0)
    implicitWidth: 232
    implicitHeight: 28
    checkable: false
    checked: Boolean(entryData.checked ?? false)
    Accessible.role: Accessible.MenuItem
    Accessible.focusable: enabled
    Accessible.name: text
    Accessible.description: String(entryData.kind ?? "action") === "submenu"
        ? qsTr("Opens submenu") : String(entryData.shortcutText ?? "")
    Accessible.checkable: Boolean(entryData.checkable ?? false)
    Accessible.checked: Boolean(entryData.checked ?? false)

    onTriggered: {
        if (String(entryData.kind ?? "action") === "action" && enabled)
            access.activate(String(entryData.id ?? ""),
                            String(entryData.generation ?? ""))
    }

    background: Rectangle {
        color: control.highlighted
            ? (control.colors.accent ?? "#8fc8b7") : "transparent"
        radius: 4
    }

    contentItem: Row {
        spacing: 8
        Text {
            width: 18
            text: Boolean(control.entryData.checkable)
                ? (Boolean(control.entryData.checked) ? "✓" : "") : ""
            color: control.highlighted
                ? (control.colors.accentText ?? "#10201b")
                : (control.colors.text ?? "white")
        }
        Text {
            width: 148
            text: control.text
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: control.enabled
                ? (control.highlighted
                   ? (control.colors.accentText ?? "#10201b")
                   : (control.colors.text ?? "white"))
                : (control.colors.textMuted ?? "#a9afa9")
        }
        Text {
            width: 42
            horizontalAlignment: Text.AlignRight
            text: String(control.entryData.kind ?? "action") === "submenu"
                ? "›" : String(control.entryData.shortcutText ?? "")
            textFormat: Text.PlainText
            color: control.highlighted
                ? (control.colors.accentText ?? "#10201b")
                : (control.colors.textMuted ?? "#a9afa9")
        }
    }
}
