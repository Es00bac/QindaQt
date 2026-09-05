// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as Shell

Item {
    id: root
    width: 900
    height: 500
    function spec(id, zone) {
        return { id: id, plugin: "task-list", settings: { zone: zone },
            runtime: { ready: true, entryPoint: "qindaqt.applets.task-list" } }
    }
    function named(item, name) {
        let result = item.objectName === name ? [item] : []
        for (let child of item.children || [])
            result = result.concat(named(child, name))
        return result
    }
    Shell.PanelContent {
        id: panel
        width: 160
        height: 32
        theme: ({ colors: {} })
        liveApplets: true
        panel: ({ edge: "top", rows: 1, applets: [] })
    }
    TestCase {
        name: "PanelGeometry"
        when: windowShown
        function init() {
            panel.width = 160
            panel.height = 32
            panel.panel = { edge: "top", rows: 1, applets: [
                root.spec("a", "start"), root.spec("b", "end")] }
            wait(20)
        }
        function test_exactlyOneRendererPerInstance() {
            compare(root.named(panel, "taskListApplet").length, 2)
            compare(root.named(panel, "appletChip").length, 2)
            compare(root.named(panel, "launcherApplet").length, 0)
            panel.panel = { edge: "left", rows: 1, applets: [root.spec("a", "center")] }
            wait(20)
            compare(root.named(panel, "taskListApplet").length, 1)
        }
        function test_desktopControlsHaveOnePurposeFacade_data() {
            return [
                { tag: "switcher", plugin: "workspace-switcher", kind: "workspaceSwitcher", facade: "workspaces" },
                { tag: "workspaces", plugin: "workspace-tiles", kind: "workspaceTiles", facade: "workspaces" },
                { tag: "desktop", plugin: "show-desktop", kind: "showDesktop", facade: "workspaces" },
                { tag: "overview", plugin: "overview-trigger", kind: "overviewTrigger", facade: "overview" },
                { tag: "active", plugin: "active-application", kind: "activeApplication", facade: "activeApplication" },
                { tag: "system", plugin: "system-menu", kind: "systemMenu", facade: "systemMenu" },
                { tag: "status", plugin: "system-status", kind: "systemStatus", facade: "systemStatus" },
                { tag: "places", plugin: "places-menu", kind: "placesMenu", facade: "places" },
                { tag: "quick", plugin: "quick-launch", kind: "quickLaunch", facade: "quickLaunch" },
                { tag: "tiles", plugin: "application-tiles", kind: "applicationTiles", facade: "quickLaunch" },
                { tag: "palette", plugin: "command-palette", kind: "commandPalette", facade: "commandPalette" },
                { tag: "hud", plugin: "command-hud", kind: "commandHud", facade: "commandHud" },
                { tag: "dashboard", plugin: "dashboard", kind: "dashboard", facade: "dashboard" }
            ]
        }
        function test_desktopControlsHaveOnePurposeFacade(data) {
            const facades = {}
            facades[data.facade] = { marker: data.facade }
            if (data.facade === "dashboard") {
                facades.systemStatus = { marker: "dashboard-status" }
                facades.workspaces = { marker: "dashboard-workspaces" }
                facades.launcher = { marker: "dashboard-launcher" }
            }
            panel.desktopControlsAccess = facades
            const applet = { id: "control", plugin: data.plugin, settings: { zone: "center" },
                runtime: { ready: true, entryPoint: "qindaqt.applets." + data.plugin } }
            panel.panel = { edge: "top", rows: 1, applets: [applet] }
            wait(20)
            let controls = root.named(panel, data.kind + "Applet")
            compare(controls.length, 1)
            if (data.facade === "dashboard") {
                compare(controls[0].access.systemStatus.marker, "dashboard-status")
                compare(controls[0].access.workspaces.marker, "dashboard-workspaces")
                compare(controls[0].access.launcher.marker, "dashboard-launcher")
            } else {
                compare(controls[0].access.marker, data.facade)
            }
            compare(controls[0].vertical, false)
            compare(root.named(panel, "launcherApplet").length, 0)
            compare(root.named(panel, "taskListApplet").length, 0)
            panel.desktopControlsAccess = null
            wait(20)
            controls = root.named(panel, data.kind + "Applet")
            compare(controls.length, 1)
            compare(controls[0].access, null)
            verify(!controls[0].enabled)
            panel.desktopControlsAccess = facades
            panel.panel = { edge: "left", rows: 1, applets: [applet] }
            wait(20)
            controls = root.named(panel, data.kind + "Applet")
            compare(controls.length, 1)
            compare(controls[0].vertical, true)
            // Swapping kind destroys the selected applet instead of retaining
            // dormant controls with borrowed live service authority.
            panel.panel = { edge: "left", rows: 1, applets: [root.spec("task", "center")] }
            wait(20)
            compare(root.named(panel, data.kind + "Applet").length, 0)
            compare(root.named(panel, "taskListApplet").length, 1)
            panel.desktopControlsAccess = null
        }
        function test_disjointZonesWithScrollableOverflow() {
            panel.panel = { edge: "top", rows: 1, applets: [
                root.spec("a", "start"), root.spec("b", "center"),
                root.spec("c", "end"), root.spec("d", "end")] }
            wait(20)
            const start = findChild(panel, "panelZoneStart")
            const center = findChild(panel, "panelZoneCenter")
            const end = findChild(panel, "panelZoneEnd")
            verify(start.x + start.width <= center.x + 0.01)
            verify(center.x + center.width <= end.x + 0.01)
            verify(end.x + end.width <= panel.width - 4 + 0.01)
            verify(end.contentWidth > end.width)
            end.contentX = 0
            const endChips = root.named(end, "appletChip")
            endChips[endChips.length - 1].forceActiveFocus()
            tryVerify(() => end.contentX > 0)
        }
        function test_longTaskZoneCannotStarveShortZone() {
            panel.width = 400
            const items = [root.spec("short", "start")]
            for (let i = 0; i < 20; ++i)
                items.push(root.spec("task" + i, "end"))
            panel.panel = { edge: "top", rows: 1, applets: items }
            wait(20)
            const start = findChild(panel, "panelZoneStart")
            compare(start.width, start.desiredExtent)
            const end = findChild(panel, "panelZoneEnd")
            verify(end.width > 0)
            verify(end.contentWidth > end.width)
        }
        function test_twoRowsAndVerticalColumns() {
            panel.height = 64
            panel.panel = { edge: "top", rows: 2, applets: [
                root.spec("a", "start"), root.spec("b", "start")] }
            wait(20)
            let chips = root.named(panel, "appletChip")
            compare(chips.length, 2)
            compare(chips[0].x, chips[1].x)
            verify(chips[0].y + chips[0].height < chips[1].y)
            compare(chips[0].height, 26)
            panel.width = 64
            panel.height = 200
            panel.panel = { edge: "left", rows: 2, applets: [
                root.spec("a", "start"), root.spec("b", "start")] }
            wait(20)
            chips = root.named(panel, "appletChip")
            compare(chips[0].y, chips[1].y)
            verify(chips[0].x + chips[0].width < chips[1].x)
            compare(chips[0].width, 26)
        }
    }
}
