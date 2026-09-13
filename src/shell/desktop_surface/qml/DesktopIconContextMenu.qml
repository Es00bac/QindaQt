// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Per-icon menu. Its 1x1 parent anchor is positioned by DesktopIconsView so
// the xdg popup appears beneath the physical pointer on Wayland.
T.Menu {
    id: root
    signal openRequested()
    signal renameRequested()
    popupType: T.Popup.Window
    topPadding: 4
    bottomPadding: 4
    implicitWidth: Math.max(180, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(1, implicitContentHeight + topPadding + bottomPadding)
    contentItem: ListView {
        implicitWidth: 180
        implicitHeight: contentHeight
        model: root.contentModel
        currentIndex: root.currentIndex
    }
    background: Rectangle {
        color: "#ee20242a"
        radius: 6
        border.color: "#3c433f"
    }

    component IconMenuItem: T.MenuItem {
        id: menuItem
        required property string label
        padding: 6
        leftPadding: 10
        rightPadding: 10
        implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, 1)
        implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding, 1)
        contentItem: Text {
            text: menuItem.label
            color: "#ffeeeeee"
            font: menuItem.font
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 3
            color: menuItem.hovered || menuItem.activeFocus ? "#3b74dd" : "transparent"
        }
        Accessible.role: Accessible.MenuItem
        Accessible.name: menuItem.label
    }

    IconMenuItem {
        objectName: "desktopIconContextOpen"
        label: qsTr("Open")
        onTriggered: root.openRequested()
    }
    IconMenuItem {
        objectName: "desktopIconContextRename"
        label: qsTr("Rename…")
        onTriggered: root.renameRequested()
    }
}
