// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T

// Meta+right-click menu on one applet chip (live customization). Entry order
// is a keyboard contract for the nested rows:
//   0 Move to start   1 Move to center   2 Move to end   3 Move left
//   4 Move right   5 Move to panel ▸   6 Remove "<applet>"   7 "<applet>" settings ▸
//   8 Duplicate "<applet>"
// Duplicate came last with the retired Settings editor (ADR-0267) so no
// earlier position moved; the nested flows' APPLET_COUNT counts it.
// Typed settings rows come from the manifest's settingsSchema through the
// controller (boolean switches, closed choices, bounded integers).
// AGENT-GUARD: never gate a row with `visible` (QQC2 Menu evicts it), and
// never disable one either: keyboard navigation skips disabled rows, which
// would shift every later position. A same-zone move, a step at the zone
// edge, or an empty submenu is a harmless no-op the controller reports.
T.Menu {
    id: root

    required property var panel
    required property var applet
    property var controller: null
    readonly property string panelId: String(panel.id ?? "")
    readonly property string appletId: String(applet.id ?? "")
    readonly property string zone: String((applet.settings ?? ({})).zone ?? "start")
    property string displayName: ""
    property var settingRows: []
    property var otherPanels: []

    objectName: "appletCustomizeMenu"
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
        displayName = controller.appletDisplayName(panelId, appletId)
        settingRows = controller.appletSettingRows(panelId, appletId)
        otherPanels = controller.panelIds().filter(id => String(id) !== panelId)
    }

    function rowsOfKind(kind) {
        return settingRows.filter(row => String(row.kind) === kind)
    }

    // Deterministic settings order (switches, choices, integers) regardless
    // of which Instantiator settles first: count the entries of the kinds
    // that precede `kind` and add the row's ordinal within its kind.
    function settingInsertionIndex(kind, ordinal) {
        const order = {boolean: 0, choice: 1, integer: 2}
        let index = 0
        for (let position = 0; position < settingsMenu.count; ++position) {
            const item = settingsMenu.itemAt(position)
            const entry = item.subMenu ? item.subMenu : item
            const entryKind = entry.settingKind !== undefined ? String(entry.settingKind) : "boolean"
            if (order[entryKind] < order[kind])
                ++index
        }
        return index + ordinal
    }

    T.MenuItem {
        objectName: "appletCustomizeMoveStart"
        text: qsTr("Move to start")
        onTriggered: root.controller.moveAppletToZone(root.panelId, root.appletId, "start")
    }
    T.MenuItem {
        objectName: "appletCustomizeMoveCenter"
        text: qsTr("Move to center")
        onTriggered: root.controller.moveAppletToZone(root.panelId, root.appletId, "center")
    }
    T.MenuItem {
        objectName: "appletCustomizeMoveEnd"
        text: qsTr("Move to end")
        onTriggered: root.controller.moveAppletToZone(root.panelId, root.appletId, "end")
    }
    T.MenuItem {
        objectName: "appletCustomizeMoveLeft"
        text: qsTr("Move left")
        onTriggered: root.controller.moveAppletStep(root.panelId, root.appletId, -1)
    }
    T.MenuItem {
        objectName: "appletCustomizeMoveRight"
        text: qsTr("Move right")
        onTriggered: root.controller.moveAppletStep(root.panelId, root.appletId, 1)
    }
    T.Menu {
        Component.onCompleted: root.pinKeyboard(this)
        id: movePanelMenu
        objectName: "appletCustomizeMovePanel"
        title: qsTr("Move to panel")
        Instantiator {
            model: root.otherPanels
            delegate: T.MenuItem {
                required property var modelData
                objectName: "appletCustomizeMovePanel:" + String(modelData)
                text: String(modelData)
                onTriggered: root.controller.moveAppletToPanel(root.panelId, root.appletId,
                                                               String(modelData))
            }
            onObjectAdded: (index, object) => movePanelMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => movePanelMenu.removeItem(object)
        }
    }
    T.MenuItem {
        objectName: "appletCustomizeRemove"
        text: qsTr("Remove \"%1\"").arg(root.displayName)
        onTriggered: root.controller.removeApplet(root.panelId, root.appletId)
    }
    T.Menu {
        Component.onCompleted: root.pinKeyboard(this)
        id: settingsMenu
        objectName: "appletCustomizeSettings"
        title: qsTr("\"%1\" settings").arg(root.displayName)

        Instantiator {
            model: root.rowsOfKind("boolean")
            delegate: T.MenuItem {
                required property var modelData
                readonly property string settingKind: "boolean"
                objectName: "appletCustomizeSetting:" + String(modelData.key)
                text: String(modelData.title)
                checkable: true
                checked: Boolean(modelData.value)
                onTriggered: root.controller.setAppletSetting(root.panelId, root.appletId,
                                                              String(modelData.key), checked)
            }
            onObjectAdded: (index, object) => settingsMenu.insertItem(
                               root.settingInsertionIndex("boolean", index), object)
            onObjectRemoved: (index, object) => settingsMenu.removeItem(object)
        }
        Instantiator {
            model: root.rowsOfKind("choice")
            delegate: T.Menu {
                Component.onCompleted: root.pinKeyboard(this)
                id: choiceMenu
                required property var modelData
                readonly property string settingKind: "choice"
                objectName: "appletCustomizeSetting:" + String(modelData.key)
                title: String(modelData.title)
                // Instantiator + insertItem is the documented dynamic path;
                // a Repeater inside a dynamically created Menu is not.
                Instantiator {
                    model: choiceMenu.modelData.choices
                    delegate: T.MenuItem {
                        required property var modelData
                        objectName: "appletCustomizeChoice:" + String(choiceMenu.modelData.key)
                                    + ":" + String(modelData)
                        text: String(modelData)
                        checkable: true
                        checked: String(choiceMenu.modelData.value) === String(modelData)
                        onTriggered: root.controller.setAppletSetting(root.panelId, root.appletId,
                                         String(choiceMenu.modelData.key), String(modelData))
                    }
                    onObjectAdded: (index, object) => choiceMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => choiceMenu.removeItem(object)
                }
            }
            onObjectAdded: (index, object) => settingsMenu.insertMenu(
                               root.settingInsertionIndex("choice", index), object)
            onObjectRemoved: (index, object) => settingsMenu.removeMenu(object)
        }
        Instantiator {
            model: root.rowsOfKind("integer")
            delegate: T.Menu {
                Component.onCompleted: root.pinKeyboard(this)
                id: integerMenu
                required property var modelData
                readonly property string settingKind: "integer"
                objectName: "appletCustomizeSetting:" + String(modelData.key)
                title: String(modelData.title)
                T.SpinBox {
                    objectName: "appletCustomizeInteger:" + String(integerMenu.modelData.key)
                    width: 220
                    from: Number(integerMenu.modelData.minimum)
                    to: Number(integerMenu.modelData.maximum)
                    stepSize: 1
                    editable: true
                    value: Number(integerMenu.modelData.value)
                    Accessible.name: String(integerMenu.modelData.title)
                    onValueModified: root.controller.setAppletSetting(root.panelId, root.appletId,
                                         String(integerMenu.modelData.key), value)
                }
            }
            onObjectAdded: (index, object) => settingsMenu.insertMenu(
                               root.settingInsertionIndex("integer", index), object)
            onObjectRemoved: (index, object) => settingsMenu.removeMenu(object)
        }
    }
    T.MenuItem {
        objectName: "appletCustomizeDuplicate"
        text: qsTr("Duplicate \"%1\"").arg(root.displayName)
        onTriggered: root.controller.duplicateApplet(root.panelId, root.appletId)
    }
}
