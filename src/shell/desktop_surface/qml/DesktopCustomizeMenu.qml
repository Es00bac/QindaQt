// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Meta+right-click menu on the desktop (live customization). Same Templates
// construction, palette and Window popup contract as DesktopContextMenu.
// Entry order is a keyboard contract for the nested rows:
//   0 Add panel ▸ (Top, Bottom, Left, Right)   1 Change wallpaper…
//   2 Desktop icons ▸ (typed desktop-icons settings)   3 Enter/Exit edit mode
//   4 Undo   5 Open Customize…
// AGENT-GUARD: rows are never `visible`-gated (QQC2 Menu evicts hidden
// items) and never disabled while the facade is present: keyboard
// navigation skips disabled rows and shifts every later position. Without
// the facade the whole menu is inert (the chord never opens it).
T.Menu {
    id: root

    // Borrowed LiveCustomizationController facade; may be null.
    property var controller: null
    readonly property bool available: controller !== null && controller.available === true
    property var iconRows: []

    objectName: "desktopCustomizeMenu"
    popupType: T.Popup.Window
    topPadding: 4
    bottomPadding: 4
    implicitWidth: Math.max(200, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(1, implicitContentHeight + topPadding + bottomPadding)

    // AGENT-GUARD: never let the view navigate keys itself (one Down would
    // step twice); keyboard positions are a contract for the nested rows.
    contentItem: ListView {
        implicitWidth: 200
        implicitHeight: contentHeight
        model: root.contentModel
        currentIndex: root.currentIndex
        interactive: false
        keyNavigationEnabled: false
        clip: true
    }
    background: Rectangle {
        color: "#ee20242a"
        radius: 6
        border.color: "#3c433f"
    }
    palette.window: "#20242a"
    palette.windowText: "#ffeeeeee"
    palette.text: "#ffeeeeee"
    palette.highlight: "#3b74dd"
    palette.highlightedText: "#ffffff"
    // Templates ship no delegate: without one a nested Menu gets no row.
    delegate: Row {}

    // Keyboard contract: a freshly opened menu has no current entry, so the
    // first Down always lands on entry 0 whichever pointer path opened it.
    onOpened: currentIndex = -1

    onAboutToShow: {
        iconRows = available
            ? controller.appletSettingRows("@desktop", "desktop-icons") : []
    }

    // Switches first, then choices, whichever Instantiator settles first.
    function iconInsertionIndex(kind, ordinal) {
        let index = 0
        if (kind === "choice") {
            for (let position = 0; position < iconsMenu.count; ++position) {
                const item = iconsMenu.itemAt(position)
                if (!item.subMenu)
                    ++index
            }
        }
        return index + ordinal
    }

    component Row: T.MenuItem {
        id: entry
        padding: 6
        leftPadding: 10
        rightPadding: 10
        hoverEnabled: true
        implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, 1)
        implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding, 1)
        contentItem: Text {
            text: entry.text + (entry.subMenu !== null ? "  ▸" : "")
            color: entry.enabled ? "#ffeeeeee" : "#7f8a8a8a"
            font: entry.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        indicator: Rectangle {
            x: entry.width - width - 8
            y: (entry.height - height) / 2
            width: 8; height: 8; radius: 4
            visible: entry.checkable
            color: entry.checked ? "#3b74dd" : "transparent"
            border.color: "#ffeeeeee"
        }
        background: Rectangle {
            radius: 3
            color: entry.enabled && (entry.hovered || entry.activeFocus) ? "#3b74dd" : "transparent"
        }
        Accessible.role: Accessible.MenuItem
        Accessible.name: entry.text
    }

    component Sub: T.Menu {
        id: sub
        popupType: T.Popup.Window
        topPadding: 4
        bottomPadding: 4
        implicitWidth: Math.max(180, implicitContentWidth + leftPadding + rightPadding)
        implicitHeight: Math.max(1, implicitContentHeight + topPadding + bottomPadding)
        // Templates menus own their content view; without one no row exists.
        contentItem: ListView {
            implicitWidth: 180
            implicitHeight: contentHeight
            model: sub.contentModel
            currentIndex: sub.currentIndex
            interactive: false
            keyNavigationEnabled: false
            clip: true
        }
        background: Rectangle { color: "#ee20242a"; radius: 6; border.color: "#3c433f" }
        palette.windowText: "#ffeeeeee"
        palette.text: "#ffeeeeee"
        delegate: Row {}
    }

    Sub {
        id: addPanelMenu
        objectName: "desktopCustomizeAddPanel"
        title: qsTr("Add panel")
        enabled: root.available
        Instantiator {
            model: ["top", "bottom", "left", "right"]
            delegate: Row {
                required property string modelData
                objectName: "desktopCustomizeAddPanel:" + modelData
                text: ({top: qsTr("At the top"), bottom: qsTr("At the bottom"),
                        left: qsTr("On the left"), right: qsTr("On the right")})[modelData]
                onTriggered: root.controller.addPanel(modelData)
            }
            onObjectAdded: (index, object) => addPanelMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => addPanelMenu.removeItem(object)
        }
    }
    Row {
        objectName: "desktopCustomizeWallpaper"
        text: qsTr("Change wallpaper…")
        enabled: root.available
        onTriggered: root.controller.openWallpaperSettings()
    }
    Sub {
        id: iconsMenu
        objectName: "desktopCustomizeIcons"
        title: qsTr("Desktop icons")
        enabled: root.available
        Instantiator {
            model: root.iconRows.filter(row => String(row.kind) === "boolean")
            delegate: Row {
                required property var modelData
                objectName: "desktopCustomizeIcons:" + String(modelData.key)
                text: String(modelData.title)
                checkable: true
                checked: Boolean(modelData.value)
                onTriggered: root.controller.setAppletSetting("@desktop", "desktop-icons",
                                                              String(modelData.key), checked)
            }
            onObjectAdded: (index, object) => iconsMenu.insertItem(
                               root.iconInsertionIndex("boolean", index), object)
            onObjectRemoved: (index, object) => iconsMenu.removeItem(object)
        }
        Instantiator {
            model: root.iconRows.filter(row => String(row.kind) === "choice")
            delegate: Sub {
                id: choiceMenu
                required property var modelData
                objectName: "desktopCustomizeIcons:" + String(modelData.key)
                title: String(modelData.title)
                Instantiator {
                    model: choiceMenu.modelData.choices
                    delegate: Row {
                        required property var modelData
                        objectName: "desktopCustomizeIconsChoice:" + String(choiceMenu.modelData.key)
                                    + ":" + String(modelData)
                        text: String(modelData)
                        checkable: true
                        checked: String(choiceMenu.modelData.value) === String(modelData)
                        onTriggered: root.controller.setAppletSetting("@desktop", "desktop-icons",
                                         String(choiceMenu.modelData.key), String(modelData))
                    }
                    onObjectAdded: (index, object) => choiceMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => choiceMenu.removeItem(object)
                }
            }
            onObjectAdded: (index, object) => iconsMenu.insertMenu(
                               root.iconInsertionIndex("choice", index), object)
            onObjectRemoved: (index, object) => iconsMenu.removeMenu(object)
        }
    }
    Row {
        objectName: "desktopCustomizeEditMode"
        text: (root.available && root.controller.editMode) ? qsTr("Exit edit mode")
                                                            : qsTr("Enter edit mode")
        enabled: root.available
        onTriggered: root.controller.toggleEditMode()
    }
    Row {
        objectName: "desktopCustomizeUndo"
        text: qsTr("Undo")
        enabled: root.available
        onTriggered: root.controller.undo()
    }
    Row {
        objectName: "desktopCustomizeOpenCustomize"
        text: qsTr("Open Customize…")
        enabled: root.available
        onTriggered: root.controller.openCustomize()
    }
}
