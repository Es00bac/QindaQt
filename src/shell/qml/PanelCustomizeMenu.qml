// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T

// Meta+right-click menu on a panel's own surface (live customization). Every
// entry is one LiveCustomizationController call, which is one engine gesture
// and one Apply; the shell adopts the written profile through its store
// watcher. Entry order is a keyboard contract for the nested rows:
//   0 Add applet ▸   1 Panel ▸   2 Add panel ▸   3 Remove panel
//   4 Enter/Exit edit mode   5 Undo   6 Redo   7 Open Customize…
// AGENT-GUARD: never gate a row with `visible` (QQC2 Menu evicts a hidden
// item from its content model for good) and never disable one: keyboard
// navigation skips disabled rows and shifts every later position. Undo with
// nothing to undo, or Open Customize without a route launcher, is a no-op
// the controller reports.
T.Menu {
    id: root

    required property var panel
    property var controller: null
    readonly property string panelId: String(panel.id ?? "")
    property var options: ({})
    property var paletteRows: []
    property var panelIds: []

    objectName: "panelCustomizeMenu"
    popupType: T.Popup.Window

    // Keyboard contract: a freshly opened menu has no current entry, so the
    // first Down always lands on entry 0 whichever pointer path opened it.
    onOpened: currentIndex = -1

    // AGENT-GUARD: the Basic style's content ListView turns `interactive`
    // (and with it its own key navigation) on whenever its content is even a
    // fraction taller than the popup window, which makes one Down key step
    // twice (ListView, then Menu). Keyboard positions are a contract for the
    // nested rows and for users, so every menu here pins the view.
    function pinKeyboard(menu) {
        if (menu.contentItem) {
            menu.contentItem.interactive = false
            menu.contentItem.keyNavigationEnabled = false
        }
    }
    Component.onCompleted: pinKeyboard(root)

    onAboutToShow: {
        if (controller === null) {
            return
        }
        options = controller.panelOptions(panelId)
        paletteRows = controller.palette(panelId)
        panelIds = controller.panelIds()
    }

    T.Menu {

        Component.onCompleted: root.pinKeyboard(this)
        id: addAppletMenu
        objectName: "panelCustomizeAddApplet"
        title: qsTr("Add applet")

        Instantiator {
            model: root.paletteRows
            delegate: T.MenuItem {
                required property var modelData
                objectName: "panelCustomizeAddApplet:" + String(modelData.pluginId)
                text: String(modelData.name)
                onTriggered: root.controller.addApplet(root.panelId,
                    String(modelData.zones[0] ?? "start"), String(modelData.pluginId))
            }
            onObjectAdded: (index, object) => addAppletMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => addAppletMenu.removeItem(object)
        }
    }

    T.Menu {

        Component.onCompleted: root.pinKeyboard(this)
        objectName: "panelCustomizePanel"
        title: qsTr("Panel")

        T.Menu {

            Component.onCompleted: root.pinKeyboard(this)
            objectName: "panelCustomizeEdge"
            title: qsTr("Edge")
            Repeater {
                model: ["top", "bottom", "left", "right"]
                T.MenuItem {
                    required property string modelData
                    objectName: "panelCustomizeEdge:" + modelData
                    text: ({top: qsTr("Top"), bottom: qsTr("Bottom"),
                            left: qsTr("Left"), right: qsTr("Right")})[modelData]
                    checkable: true
                    checked: String(root.options.edge ?? "") === modelData
                    onTriggered: root.controller.configurePanel(root.panelId, "edge", modelData)
                }
            }
        }
        T.Menu {
            Component.onCompleted: root.pinKeyboard(this)
            objectName: "panelCustomizeAlignment"
            title: qsTr("Alignment")
            Repeater {
                model: ["fill", "start", "center", "end"]
                T.MenuItem {
                    required property string modelData
                    objectName: "panelCustomizeAlignment:" + modelData
                    text: ({fill: qsTr("Fill"), start: qsTr("Start"),
                            center: qsTr("Center"), end: qsTr("End")})[modelData]
                    checkable: true
                    checked: String(root.options.alignment ?? "") === modelData
                    onTriggered: root.controller.configurePanel(root.panelId, "alignment", modelData)
                }
            }
        }
        T.Menu {
            Component.onCompleted: root.pinKeyboard(this)
            objectName: "panelCustomizeHideMode"
            title: qsTr("Auto-hide")
            Repeater {
                model: ["never", "intelligent", "dodge-active", "dodge-all", "maximized", "always"]
                T.MenuItem {
                    required property string modelData
                    objectName: "panelCustomizeHideMode:" + modelData
                    text: ({never: qsTr("Never"), intelligent: qsTr("Intelligent"),
                            "dodge-active": qsTr("Dodge active window"),
                            "dodge-all": qsTr("Dodge any window"),
                            maximized: qsTr("When a window is maximized"),
                            always: qsTr("Always")})[modelData]
                    checkable: true
                    checked: String(root.options.hideMode ?? "") === modelData
                    onTriggered: root.controller.configurePanel(root.panelId, "hideMode", modelData)
                }
            }
        }
        T.Menu {
            Component.onCompleted: root.pinKeyboard(this)
            objectName: "panelCustomizeSize"
            title: qsTr("Size (logical px)")
            T.SpinBox {
                objectName: "panelCustomizeSizeInput"
                width: 220; from: 16; to: 160; stepSize: 1
                editable: true
                value: Number(root.options.thickness ?? 32)
                Accessible.name: qsTr("Panel thickness in logical pixels")
                onValueModified: root.controller.configurePanel(root.panelId, "thickness", value)
            }
        }
        T.Menu {
            Component.onCompleted: root.pinKeyboard(this)
            objectName: "panelCustomizeLength"
            title: qsTr("Length (percent)")
            T.SpinBox {
                objectName: "panelCustomizeLengthInput"
                width: 220; from: 10; to: 100; stepSize: 5
                editable: true
                value: Math.round(Number(root.options.length ?? 1) * 100)
                Accessible.name: qsTr("Panel length as a percentage of the edge")
                onValueModified: root.controller.configurePanel(root.panelId, "length", value / 100)
            }
        }
    }

    T.Menu {

        Component.onCompleted: root.pinKeyboard(this)
        objectName: "panelCustomizeAddPanel"
        title: qsTr("Add panel")
        Repeater {
            model: ["top", "bottom", "left", "right"]
            T.MenuItem {
                required property string modelData
                objectName: "panelCustomizeAddPanel:" + modelData
                text: ({top: qsTr("At the top"), bottom: qsTr("At the bottom"),
                        left: qsTr("On the left"), right: qsTr("On the right")})[modelData]
                onTriggered: root.controller.addPanel(modelData)
            }
        }
    }
    T.MenuItem {
        objectName: "panelCustomizeRemovePanel"
        text: qsTr("Remove panel")
        onTriggered: root.controller.removePanel(root.panelId)
    }
    T.MenuItem {
        objectName: "panelCustomizeEditMode"
        text: (root.controller !== null && root.controller.editMode) ? qsTr("Exit edit mode") : qsTr("Enter edit mode")
        onTriggered: root.controller.toggleEditMode()
    }
    T.MenuItem {
        objectName: "panelCustomizeUndo"
        text: qsTr("Undo")
        onTriggered: root.controller.undo()
    }
    T.MenuItem {
        objectName: "panelCustomizeRedo"
        text: qsTr("Redo")
        onTriggered: root.controller.redo()
    }
    T.MenuItem {
        objectName: "panelCustomizeOpenCustomize"
        text: qsTr("Open Customize…")
        onTriggered: root.controller.openCustomize()
    }
}
