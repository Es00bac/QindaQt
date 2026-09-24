// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as Shell

// Live customization edit mode on the panel surface (O9, ADR-0266): Edit
// Panels from the plain right-click menu, inert applets that drag as a whole,
// the bar with its applet picker beside (never over) the applets, drop
// targets that resolve zones and anchors, drags that cross to another panel
// surface through the global drag point, off-target releases that cancel, and
// the drop preview's gap and marker. Same recording stub as
// tst_livecustomization.qml, plus a second panel surface ("tray") below the
// bar; the stub's solved geometry puts both surfaces where they sit here.
Item {
    id: root
    width: 700
    height: 160

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

    function trayFixture() {
        return { id: "tray", edge: "bottom", alignment: "fill", rows: 1, thickness: 40,
            applets: [
                { id: "t", plugin: "clock", settings: { zone: "end" },
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
        property var dropTarget: ({})
        property point dragPoint: Qt.point(0, 0)
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
        function appletSettingRows(p, a) { return [] }
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
        function hoverDropTarget(p, z, b) {
            dropAccepted = p !== ""
            dropTarget = dropAccepted ? {panelId: p, zone: z, beforeAppletId: b} : ({})
            return record("hoverDropTarget", p, z, b)
        }
        function dropApplet() { dragActive = false; dropAccepted = false; return record("dropApplet") }
        function cancelDrag() { dragActive = false; dropAccepted = false; return record("cancelDrag") }
        // The controller's contract: over no panel surface an accepted target
        // is replaced by the empty one first; the point is announced always.
        function trackDragPoint(x, y) {
            record("trackDragPoint", x, y)
            if (dropAccepted && panelSurfaceAt("", x, y).panelId === undefined)
                hoverDropTarget("", "", "")
            dragPoint = Qt.point(x, y)
        }
        function panelSurface(o, p) {
            return p === "tray"
                ? {panelId: "tray", x: 0, y: 100, width: 640, height: 40, horizontal: true}
                : {panelId: "bar", x: 0, y: 0, width: 640, height: 32, horizontal: true}
        }
        function panelSurfaceAt(o, x, y) {
            if (x < 0 || x >= 640)
                return {}
            return y >= 0 && y < 32 ? panelSurface(o, "bar")
                 : y >= 100 && y < 140 ? panelSurface(o, "tray") : {}
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

    Shell.PanelContent {
        id: tray
        y: 100
        width: 640
        height: 40
        theme: ({ colors: {} })
        liveApplets: false
        panel: root.trayFixture()
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
            stub.dropTarget = ({})
            panel.panel = root.fixture()
            tray.panel = root.trayFixture()
            wait(20)
        }

        function menu(name) {
            const found = findChild(panel, name)
            verify(found !== null && found !== undefined, "menu " + name + " exists")
            return found
        }

        function chips(surface) {
            return root.named(surface ?? panel, "appletChip")
        }

        function lastCall(name) {
            const matching = stub.calls.filter(call => call[0] === name)
            return matching.length > 0 ? matching[matching.length - 1] : []
        }

        function test_editModeShowsHandlesAndBar() {
            const bar = root.named(panel, "panelEditBar")[0]
            verify(!bar.visible)
            const frames = root.named(panel, "appletEditHandleFrame")
            compare(frames.length, 3)
            verify(!frames[0].visible)
            const endZone = root.named(panel, "panelZoneEnd")[0]
            const endBefore = endZone.x + endZone.width
            stub.enterEditMode()
            tryVerify(() => bar.visible, 2000)
            verify(frames[0].visible)
            verify(bar.x + bar.width <= panel.width)
            // The bar has its own stretch: the end zone ends before it.
            tryVerify(() => endZone.x + endZone.width <= bar.x, 2000)
            verify(endZone.x + endZone.width < endBefore)
            const add = root.named(panel, "panelEditAddApplet")[0]
            const done = root.named(panel, "panelEditDone")[0]
            const undo = root.named(panel, "panelEditUndo")[0]
            verify(add.enabled)
            verify(undo.enabled)
            undo.clicked()
            compare(stub.calls[stub.calls.length - 1], ["undo"])
            done.clicked()
            compare(stub.calls[stub.calls.length - 1], ["exitEditMode"])
            tryVerify(() => !bar.visible, 2000)
            verify(!frames[0].visible)
            tryVerify(() => Math.abs(endZone.x + endZone.width - endBefore) < 0.5, 2000)
        }

        function test_plainRightClickOffersEditPanels() {
            const quick = menu("panelConfigMenu")
            mouseClick(panel, 320, 16, Qt.RightButton, Qt.NoModifier)
            tryVerify(() => quick.opened, 2000)
            const entry = quick.itemAt(0)
            compare(entry.objectName, "panelConfigEditPanels")
            compare(entry.text, "Edit Panels")
            verify(entry.enabled)
            entry.triggered()
            compare(stub.calls[stub.calls.length - 1], ["toggleEditMode"])
            verify(stub.editMode)
            compare(entry.text, "Done Editing Panels")
            quick.close()
            tryVerify(() => !quick.opened, 2000)
        }

        function test_appletsAreInertInEditMode() {
            const first = chips()[0]
            const handle = root.named(panel, "appletEditHandle")[0]
            const shield = root.named(panel, "appletEditShield")[0]
            const appletMenu = findChild(handle, "appletCustomizeMenu:a")
            // Outside edit mode the shield is off and presses reach the applet.
            verify(!shield.enabled)
            stub.enterEditMode()
            tryVerify(() => shield.enabled, 2000)
            // In edit mode the shield owns every press: a plain right click
            // opens the customize menu, a left click does nothing at all.
            mouseClick(first, first.width / 2, first.height / 2, Qt.LeftButton, Qt.NoModifier)
            compare(stub.calls.length, 1)
            mouseClick(first, first.width / 2, first.height / 2, Qt.RightButton, Qt.NoModifier)
            tryVerify(() => appletMenu.opened, 2000)
            appletMenu.close()
            tryVerify(() => !appletMenu.opened, 2000)
        }

        function test_dropTargetsResolveZonesAndAnchors() {
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
            // The middle of the bar is the (empty) center zone: zone
            // boundaries sit halfway between zones, so it stays reachable.
            target = panel.dropTargetAt(panel.width / 2, 16)
            compare(target.zone, "center")
            compare(target.beforeAppletId, "")
            // The far right is the end zone; before its only chip.
            const c = items[2].mapToItem(panel, items[2].width / 2, items[2].height / 2)
            target = panel.dropTargetAt(c.x - items[2].width / 2 + 1, 16)
            compare(target.zone, "end")
            compare(target.beforeAppletId, "c")
            // Outside this surface there is no target here: the surface under
            // the global drag point resolves it.
            verify(panel.dropTargetAt(600, 110) === null)
            verify(panel.dropTargetAt(-4, 16) === null)
        }

        function test_dragWithinThePanelDrivesTheController() {
            stub.enterEditMode()
            wait(20)
            const first = chips()[0]
            mouseDrag(first, first.width / 2, first.height / 2, 160, 0, Qt.LeftButton)
            tryVerify(() => !stub.dragActive, 2000)
            compare(stub.calls[1], ["beginAppletDrag", "bar", "a"])
            verify(stub.calls.some(call => call[0] === "trackDragPoint"))
            compare(lastCall("hoverDropTarget")[1], "bar")
            compare(stub.calls[stub.calls.length - 1], ["dropApplet"])
        }

        function test_dragOntoAnotherPanelResolvesThere() {
            stub.enterEditMode()
            wait(20)
            const first = chips()[0]
            // Down onto the tray's trailing end: past its only chip.
            mouseDrag(first, first.width / 2, first.height / 2,
                      620 - first.width / 2, 116 - first.height / 2, Qt.LeftButton)
            tryVerify(() => !stub.dragActive, 2000)
            compare(lastCall("hoverDropTarget"), ["hoverDropTarget", "tray", "end", ""])
            compare(stub.calls[stub.calls.length - 1], ["dropApplet"])
        }

        function test_releaseOverNoPanelCancels() {
            stub.enterEditMode()
            wait(20)
            const first = chips()[0]
            mouseDrag(first, first.width / 2, first.height / 2,
                      300 - first.width / 2, 66 - first.height / 2, Qt.LeftButton)
            tryVerify(() => !stub.dragActive, 2000)
            compare(lastCall("hoverDropTarget"), ["hoverDropTarget", "", "", ""])
            compare(stub.calls[stub.calls.length - 1], ["cancelDrag"])
        }

        function test_dropPreviewOpensAGapWithAMarker() {
            const items = chips()
            const marker = root.named(panel, "panelDropMarker")[0]
            const aBefore = items[0].mapToItem(panel, 0, 0).x
            const bBefore = items[1].mapToItem(panel, 0, 0).x
            verify(!marker.visible)
            stub.dragActive = true
            stub.hoverDropTarget("bar", "start", "b")
            tryVerify(() => marker.visible, 2000)
            // a stays, b makes room, and the marker sits in the gap between.
            tryVerify(() => items[1].mapToItem(panel, 0, 0).x > bBefore, 2000)
            compare(items[0].mapToItem(panel, 0, 0).x, aBefore)
            const markerX = marker.mapToItem(panel, marker.width / 2, 0).x
            verify(markerX > aBefore + items[0].width)
            verify(markerX < items[1].mapToItem(panel, 0, 0).x)
            // Another panel's target leaves this one untouched.
            stub.hoverDropTarget("tray", "end", "")
            tryVerify(() => !marker.visible, 2000)
            tryVerify(() => Math.abs(items[1].mapToItem(panel, 0, 0).x - bBefore) < 0.5, 2000)
            verify(root.named(tray, "panelDropMarker").some(item => item.visible))
            stub.cancelDrag()
            tryVerify(() => !root.named(tray, "panelDropMarker").some(item => item.visible), 2000)
        }

        function test_addAppletPickerAddsToTheChosenZone() {
            stub.enterEditMode()
            const add = root.named(panel, "panelEditAddApplet")[0]
            tryVerify(() => add.visible, 2000)
            const picker = findChild(root.named(panel, "panelEditBar")[0], "panelAppletPicker")
            verify(picker !== null && picker !== undefined)
            add.clicked()
            tryVerify(() => picker.opened, 2000)
            tryCompare(picker, "count", 3)
            // The catalog admits the clock at the start and the end only.
            verify(picker.menuAt(0).enabled)
            verify(!picker.menuAt(1).enabled)
            const endZone = picker.menuAt(2)
            compare(endZone.title, "Right")
            tryCompare(endZone, "count", 1)
            endZone.itemAt(0).triggered()
            compare(stub.calls[stub.calls.length - 1], ["addApplet", "bar", "end", "clock"])
            picker.close()
            tryVerify(() => !picker.opened, 2000)
        }
    }
}
