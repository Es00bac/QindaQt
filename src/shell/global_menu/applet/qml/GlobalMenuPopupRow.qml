// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// One row of GlobalMenuPopup's ListView: a separator line or a clickable
// action/submenu entry. Presentation only — activation travels upward through
// the activated signal so GlobalMenuPopup keeps its one-gesture-one-facade-
// call guard (AGENT-GUARD on choose) in a single place.
Item {
    id: row

    required property var modelData
    required property int index
    required property int currentIndex
    required property real listWidth
    required property var colors

    signal activated(var item)

    readonly property string kind: String(modelData.kind ?? "action")
    readonly property bool separator: kind === "separator"
    readonly property bool current: currentIndex === index

    objectName: separator ? "globalMenuPopupSeparator"
                          : "globalMenuPopupItem"
    width: listWidth
    height: separator ? 9 : 28

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 6
        height: 1
        visible: row.separator
        color: row.colors.border ?? "#3c433f"
    }

    AbstractButton {
        id: button
        objectName: "globalMenuPopupButton"
        anchors.fill: parent
        visible: !row.separator
        enabled: Boolean(row.modelData.enabled)
        checkable: false
        checked: Boolean(row.modelData.checked ?? false)
        hoverEnabled: true
        Accessible.role: Accessible.MenuItem
        Accessible.focusable: enabled
        Accessible.name: String(row.modelData.text ?? "")
        Accessible.description: row.kind === "submenu"
            ? qsTr("Opens submenu")
            : String(row.modelData.shortcutText ?? "")
        Accessible.checkable: Boolean(row.modelData.checkable ?? false)
        Accessible.checked: Boolean(row.modelData.checked ?? false)
        onClicked: row.activated(row.modelData)
        Accessible.onPressAction: row.activated(row.modelData)

        background: Rectangle {
            color: button.hovered || row.current
                   ? row.colors.accent ?? "#8fc8b7" : "transparent"
            radius: 4
        }

        contentItem: Row {
            spacing: 8
            Text {
                width: 18
                text: Boolean(row.modelData.checkable)
                      ? (Boolean(row.modelData.checked) ? "✓" : "") : ""
                color: button.hovered || row.current
                       ? row.colors.accentText ?? "#10201b"
                       : row.colors.text ?? "white"
            }
            Text {
                width: Math.max(90, row.listWidth - 92)
                text: String(row.modelData.text ?? "")
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: button.enabled
                       ? (button.hovered || row.current
                          ? row.colors.accentText ?? "#10201b"
                          : row.colors.text ?? "white")
                       : row.colors.textMuted ?? "#a9afa9"
            }
            Text {
                width: 42
                horizontalAlignment: Text.AlignRight
                text: row.kind === "submenu" ? "›"
                      : String(row.modelData.shortcutText ?? "")
                textFormat: Text.PlainText
                color: row.colors.textMuted ?? "#a9afa9"
            }
        }
    }
}
