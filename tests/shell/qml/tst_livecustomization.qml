// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as Shell

// Live customization on the panel surface (O9): the chord opens the panel
// and applet menus while a plain right click keeps the quick-config menu,
// every menu entry is one controller call, edit mode paints handles and the
// Done/Undo bar, and the drop-target arithmetic resolves zones and anchors.
Item {
    id: root
    width: 700
    height: 120

    function named(item, name) {
        let result = item.objectName === name ? [item] : []
        for (let child of item.children || [])
            result = result.concat(named(child, name))
        return result
    }

    function fixture() {
        return { id: "bar", edge: "top", alignment: "fill", rows: 1, thickness: 32,
            applets: [
                { id: "a", plugin: "clock", settings: { zone: "start" },
                  runtime: { ready: false } },
                { id: "b", plugin: "clock", settings: { zone: "start" },
                  runtime: { ready: false } },
                { id: "c", plugin: "clock", settings: { zone: "end" },
                  runtime: { ready: false } }
            ] }
    }

    // Recording stand-in for LiveCustomizationController.
    QtObject {
        id: stub
        objectName: "stubController"
        property bool available: true
        property bool editMode: false
        property bool canUndo: true
        property bool canRedo: false
        property bool dragActive: false
        property bool dropAccepted: false
        property string dropReason: ""
        property string statusText: ""
        property string chord: "meta-right"
        property int chordModifiers: Qt.MetaModifier
        property bool customizeRouteAvailable: true
        property var calls: []
        function record() { calls = calls.concat([Array.prototype.slice.call(arguments)]); return true }
        function appletDisplayName(p, a) { return "Clock " + a }
        function moveAppletToZone(p, a, z) { return record("moveAppletToZone", p, a, z) }
        function moveAppletStep(p, a, d) { return record("moveAppletStep", p, a, d) }
        function moveAppletToPanel(p, a, t) { return record("moveAppletToPanel", p, a, t) }
        function removeApplet(p, a) { return record("removeApplet", p, a) }
        function appletSettingRows(p, a) {
            return [{key: "showSeconds", title: "Show seconds", kind: "boolean", value: false},
                    {key: "format", title: "Format", kind: "choice", value: "locale",
                     choices: ["locale", "24-hour"]},
                    {key: "size", title: "Size", kind: "integer", value: 20, minimum: 8, maximum: 40}]
        }
        function setAppletSetting(p, a, k, v) { return record("setAppletSetting", p, a, k, v) }
        function palette(p) { return [{pluginId: "clock", name: "Clock", zones: ["start", "end"]}] }
        function addApplet(p, z, id) { return record("addApplet", p, z, id) }
        function panelOptions(p) { return {edge: "top", alignment: "fill", hideMode: "never",
                                           thickness: 32, length: 1.0} }
        function panelIds() { return ["bar", "tray"] }
        function configurePanel(p, f, v) { return record("configurePanel", p, f, v) }
        function addPanel(e) { return record("addPanel", e) }
        function removePanel(p) { return record("removePanel", p) }
        function undo() { return record("undo") }
        function redo() { return record("redo") }
        function enterEditMode() { editMode = true; record("enterEditMode") }
        function exitEditMode() { editMode = false; record("exitEditMode") }
        function toggleEditMode() { editMode = !editMode; record("toggleEditMode") }
        function openCustomize() { return record("openCustomize") }
        function beginAppletDrag(p, a) { dragActive = true; return record("beginAppletDrag", p, a) }
        function hoverDropTarget(p, z, b) { dropAccepted = true; return record("hoverDropTarget", p, z, b) }
        function dropApplet() { dragActive = false; return record("dropApplet") }
        function cancelDrag() { dragActive = false; return record("cancelDrag") }
        function panelSurface(o, p) { return {panelId: p, x: 0, y: 0, width: 700, height: 32, horizontal: true} }
        function panelSurfaceAt(o, x, y) {
            return y > 100 ? {panelId: "tray", x: 0, y: 100, width: 700, height: 40, horizontal: true} : {}
        }
    }

    QtObject {
        id: quickConfig
        signal panelSettingsChanged()
        property var calls: []
        function panelSettings(id) { return {} }
        function setPanelSetting(id, key, value) { calls = calls.concat([[id, key, value]]); return true }
        function openCustomize() { return true }
    }

    Shell.PanelContent {
        id: panel
        width: 640
        height: 32
        theme: ({ colors: {} })
        liveApplets: false
        panel: root.fixture()
        panelQuickConfig: quickConfig
        liveCustomization: stub
        outputId: "OUT-1"
    }

    TestCase {
        name: "LiveCustomization"
        when: windowShown

        function init() {
            stub.calls = []
            stub.editMode = false
            stub.dragActive = false
            stub.dropAccepted = false
            panel.panel = root.fixture()
            wait(20)
        }

        // Popups are QObject children, not visual children: findChild.
        function menu(name) {
            const found = findChild(panel, name)
            verify(found !== null && found !== undefined, "menu " + name + " exists")
            return found
        }

        function chips() {
            return root.named(panel, "appletChip")
        }

        // A submenu's own row is an auto-created MenuItem; its identity is
        // the submenu's objectName.
        function entryName(menuObject, index) {
            const item = menuObject.itemAt(index)
            return item.subMenu ? item.subMenu.objectName : item.objectName
        }

        function test_chordOnPanelOpensCustomizeMenuPlainClickKeepsQuickConfig() {
            const customize = menu("panelCustomizeMenu")
            const quick = menu("panelConfigMenu")
            // An empty stretch of the panel material: between the zones.
            const point = Qt.point(320, 16)
            mouseClick(panel, point.x, point.y, Qt.RightButton, Qt.MetaModifier)
            tryVerify(() => customize.opened, 2000)
            verify(!quick.opened)
            customize.close()
            tryVerify(() => !customize.opened, 2000)
            mouseClick(panel, point.x, point.y, Qt.RightButton, Qt.NoModifier)
            tryVerify(() => quick.opened, 2000)
            verify(!customize.opened)
            quick.close()
            tryVerify(() => !quick.opened, 2000)
        }

        function test_chordIsInertWithoutTheController() {
            panel.liveCustomization = null
            const quick = menu("panelConfigMenu")
            mouseClick(panel, 320, 16, Qt.RightButton, Qt.MetaModifier)
            tryVerify(() => quick.opened, 2000)
            quick.close()
            tryVerify(() => !quick.opened, 2000)
            panel.liveCustomization = stub
        }

        function test_chordOnChipOpensAppletMenuAndEntriesCallTheController() {
            const first = chips()[0]
            const centre = first.mapToItem(panel, first.width / 2, first.height / 2)
            mouseClick(panel, centre.x, centre.y, Qt.RightButton, Qt.MetaModifier)
            // Delegate-created chips are not QObject children of the panel:
            // reach the menu through its handle.
            const handles = root.named(panel, "appletEditHandle")
            compare(handles.length, 3)
            const open = findChild(handles[0], "appletCustomizeMenu:a")
            verify(open !== null && open !== undefined)
            tryVerify(() => open.opened, 2000)
            verify(!findChild(handles[1], "appletCustomizeMenu:b").opened)
            compare(open.appletId, "a")
            compare(open.displayName, "Clock a")
            // Keyboard-parity contract: entries by objectName in the documented order.
            const names = ["appletCustomizeMoveStart", "appletCustomizeMoveCenter",
                           "appletCustomizeMoveEnd", "appletCustomizeMoveLeft",
                           "appletCustomizeMoveRight", "appletCustomizeMovePanel",
                           "appletCustomizeRemove", "appletCustomizeSettings"]
            for (let index = 0; index < names.length; ++index)
                compare(entryName(open, index), names[index], "entry " + index)
            // Every row stays enabled: keyboard positions are a strict contract.
            for (let index = 0; index < names.length; ++index)
                verify(open.itemAt(index).enabled, "enabled " + index)
            open.itemAt(2).triggered()
            open.itemAt(3).triggered()
            open.itemAt(6).triggered()
            compare(stub.calls[0], ["moveAppletToZone", "bar", "a", "end"])
            compare(stub.calls[1], ["moveAppletStep", "bar", "a", -1])
            compare(stub.calls[2], ["removeApplet", "bar", "a"])
            // The settings submenu instantiates the typed rows: switch, choice, integer.
            const settings = open.menuAt(7)
            verify(settings !== null)
            tryCompare(settings, "count", 3)
            settings.itemAt(0).triggered()
            compare(stub.calls[3].slice(0, 4), ["setAppletSetting", "bar", "a", "showSeconds"])
            compare(entryName(settings, 0), "appletCustomizeSetting:showSeconds")
            compare(entryName(settings, 1), "appletCustomizeSetting:format")
            compare(entryName(settings, 2), "appletCustomizeSetting:size")
            const choice = settings.menuAt(1)
            verify(choice !== null)
            tryCompare(choice, "count", 2)
            choice.itemAt(1).triggered()
            compare(stub.calls[4], ["setAppletSetting", "bar", "a", "format", "24-hour"])
            const movePanel = open.menuAt(5)
            tryCompare(movePanel, "count", 1)
            movePanel.itemAt(0).triggered()
            compare(stub.calls[5], ["moveAppletToPanel", "bar", "a", "tray"])
            open.close()
            tryVerify(() => !open.opened, 2000)
        }

        function test_panelMenuEntriesCallTheController() {
            const customize = menu("panelCustomizeMenu")
            mouseClick(panel, 320, 16, Qt.RightButton, Qt.MetaModifier)
            tryVerify(() => customize.opened, 2000)
            const names = ["panelCustomizeAddApplet", "panelCustomizePanel", "panelCustomizeAddPanel",
                           "panelCustomizeRemovePanel", "panelCustomizeEditMode", "panelCustomizeUndo",
                           "panelCustomizeRedo", "panelCustomizeOpenCustomize"]
            for (let index = 0; index < names.length; ++index)
                compare(entryName(customize, index), names[index], "entry " + index)
            const addApplet = customize.menuAt(0)
            tryCompare(addApplet, "count", 1)
            addApplet.itemAt(0).triggered()
            compare(stub.calls[0], ["addApplet", "bar", "start", "clock"])
            customize.menuAt(2).itemAt(1).triggered()
            compare(stub.calls[1], ["addPanel", "bottom"])
            customize.itemAt(3).triggered()
            compare(stub.calls[2], ["removePanel", "bar"])
            customize.itemAt(4).triggered()
            compare(stub.calls[3], ["toggleEditMode"])
            verify(stub.editMode)
            customize.itemAt(5).triggered()
            compare(stub.calls[4], ["undo"])
            for (let index = 0; index < names.length; ++index)
                verify(customize.itemAt(index).enabled, "enabled " + index)
            customize.itemAt(7).triggered()
            compare(stub.calls[5], ["openCustomize"])
            const panelMenu = customize.menuAt(1)
            panelMenu.menuAt(0).itemAt(1).triggered()   // Edge > Bottom
            compare(stub.calls[6], ["configurePanel", "bar", "edge", "bottom"])
            panelMenu.menuAt(2).itemAt(1).triggered()   // Auto-hide > Intelligent
            compare(stub.calls[7], ["configurePanel", "bar", "hideMode", "intelligent"])
            customize.close()
            tryVerify(() => !customize.opened, 2000)
        }
    }
}
