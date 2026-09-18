// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as Shell

// Live customization edit mode on the panel surface (O9): handles and the
// Done/Undo bar appear, drop targets resolve zones, anchors and other
// panels, and a handle drag drives the controller's drag protocol. Same
// fixture and recording stub as tst_livecustomization.qml.
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
        name: "LiveCustomizationEditMode"
        when: windowShown

        function init() {
            stub.calls = []
            stub.editMode = false
            stub.dragActive = false
            stub.dropAccepted = false
            panel.panel = root.fixture()
            wait(20)
        }

        function menu(name) {
            const found = findChild(panel, name)
            verify(found !== null && found !== undefined, "menu " + name + " exists")
            return found
        }

        function chips() {
            return root.named(panel, "appletChip")
        }

        function test_editModeShowsHandlesAndBar() {
            const bar = menu("panelEditBar")
            verify(!bar.visible)
            const frames = root.named(panel, "appletEditHandleFrame")
            compare(frames.length, 3)
            verify(!frames[0].visible)
            stub.enterEditMode()
            tryVerify(() => bar.visible, 2000)
            verify(frames[0].visible)
            verify(bar.x + bar.width <= panel.width)
            const done = root.named(panel, "panelEditDone")[0]
            const undo = root.named(panel, "panelEditUndo")[0]
            verify(undo.enabled)
            undo.clicked()
            compare(stub.calls[stub.calls.length - 1], ["undo"])
            done.clicked()
            compare(stub.calls[stub.calls.length - 1], ["exitEditMode"])
            tryVerify(() => !bar.visible, 2000)
            verify(!frames[0].visible)
        }

        function test_dropTargetsResolveZonesAnchorsAndOtherPanels() {
            const items = chips()
            const b = items[1].mapToItem(panel, items[1].width / 2, items[1].height / 2)
            // Left of b's centre inside the start zone: lands before b.
            let target = panel.dropTargetAt(b.x - items[1].width / 2 - 2, 16)
            compare(target.panelId, "bar")
            compare(target.zone, "start")
            compare(target.beforeAppletId, "b")
            // Right of b's centre, still on b: appends to the start zone.
            target = panel.dropTargetAt(b.x + items[1].width / 2 - 2, 16)
            compare(target.zone, "start")
            compare(target.beforeAppletId, "")
            // The empty stretch between the zones is the (empty) center zone.
            target = panel.dropTargetAt(b.x + items[1].width / 2 + 40, 16)
            compare(target.zone, "center")
            compare(target.beforeAppletId, "")
            // The far right is the end zone; before its only chip.
            const c = items[2].mapToItem(panel, items[2].width / 2, items[2].height / 2)
            target = panel.dropTargetAt(c.x - items[2].width / 2 + 1, 16)
            compare(target.zone, "end")
            compare(target.beforeAppletId, "c")
            // Outside this surface: another panel by solved geometry, thirds.
            target = panel.dropTargetAt(600, 110)
            compare(target.panelId, "tray")
            compare(target.zone, "end")
            compare(target.beforeAppletId, "")
            verify(panel.dropTargetAt(300, 60) === null)
        }

        function test_dragHandleDrivesTheController() {
            stub.enterEditMode()
            wait(20)
            const first = chips()[0]
            mouseDrag(first, first.width / 2, first.height / 2, 160, 0, Qt.LeftButton)
            tryVerify(() => !stub.dragActive, 2000)
            compare(stub.calls[1], ["beginAppletDrag", "bar", "a"])
            verify(stub.calls.some(call => call[0] === "hoverDropTarget" && call[1] === "bar"))
            compare(stub.calls[stub.calls.length - 1], ["dropApplet"])
        }
    }
}
