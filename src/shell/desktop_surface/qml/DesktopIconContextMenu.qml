// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Per-icon menu. Its 1x1 parent anchor is positioned by DesktopIconsView so
// the xdg popup appears beneath the physical pointer on Wayland. The menu
// opens with exactly one icon or a whole multi-selection targeted (the view
// selects on right-press), so Cut/Copy/Delete act on the view's current
// selection, mirroring the File Manager's own context menu.
T.Menu {
    id: root
    signal openRequested()
    signal renameRequested()
    signal cutRequested()
    signal copyRequested()
    signal deleteRequested()
    // ADR-0265: offered only where a dock is composed. A desktop entry reads
    // "Pin to Dock" (it becomes its installed application); anything else
    // is kept as a file or folder item.
    signal addToDockRequested()
    property bool dockAvailable: false
    property bool desktopEntry: false
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
        objectName: "desktopIconContextCut"
        label: qsTr("Cut")
        onTriggered: root.cutRequested()
    }
    IconMenuItem {
        objectName: "desktopIconContextCopy"
        label: qsTr("Copy")
        onTriggered: root.copyRequested()
    }
    IconMenuItem {
        objectName: "desktopIconContextRename"
        label: qsTr("Rename…")
        onTriggered: root.renameRequested()
    }
    IconMenuItem {
        objectName: "desktopIconContextDelete"
        label: qsTr("Delete")
        onTriggered: root.deleteRequested()
    }
    IconMenuItem {
        objectName: "desktopIconContextAddToDock"
        visible: root.dockAvailable
        height: visible ? implicitHeight : 0
        label: root.desktopEntry ? qsTr("Pin to Dock") : qsTr("Add to Dock")
        onTriggered: root.addToDockRequested()
    }
}
