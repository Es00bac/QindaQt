// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// One row of the desktop's menus (DesktopContextMenu, DesktopIconContextMenu),
// drawn from a descriptor: {objectName, text} for an entry, {separator: true}
// for a divider. The owning menu's Instantiator supplies the descriptor, sets
// `available` from its facades, and handles `triggered`. Both menus share it
// so a desktop entry looks the same wherever it is offered.
T.MenuItem {
    id: entry

    required property var modelData
    // False disables the entry: its facade is absent or refuses.
    property bool available: true

    readonly property bool isSeparator: modelData.separator === true

    objectName: isSeparator ? "" : String(modelData.objectName)
    text: isSeparator ? "" : String(modelData.text ?? "")
    enabled: !isSeparator && available
    hoverEnabled: !isSeparator
    padding: isSeparator ? 2 : 6
    leftPadding: isSeparator ? 2 : 10
    rightPadding: isSeparator ? 2 : 10
    implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, 1)
    implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding, 1)

    contentItem: Item {
        implicitWidth: entry.isSeparator ? 160 : labelText.implicitWidth
        implicitHeight: entry.isSeparator ? 1 : labelText.implicitHeight

        Rectangle {
            anchors.fill: parent
            visible: entry.isSeparator
            color: "#3c433f"
        }

        Text {
            id: labelText
            visible: !entry.isSeparator
            text: entry.text
            color: entry.enabled ? "#ffeeeeee" : "#7f8a8a8a"
            font: entry.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
    background: Rectangle {
        visible: !entry.isSeparator
        radius: 3
        color: entry.enabled && (entry.hovered || entry.activeFocus) ? "#3b74dd" : "transparent"
    }

    Accessible.role: entry.isSeparator ? Accessible.NoRole : Accessible.MenuItem
    Accessible.name: entry.text
}
