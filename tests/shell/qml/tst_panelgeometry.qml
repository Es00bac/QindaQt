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
    function deepestVisibleItemAt(item, sceneX, sceneY) {
        const local = item.mapFromItem(null, sceneX, sceneY)
        if (local.x < 0 || local.y < 0 || local.x >= item.width || local.y >= item.height)
            return null
        let deepest = item
        for (let child of item.children || []) {
            if (!child.visible)
                continue
            const hit = deepestVisibleItemAt(child, sceneX, sceneY)
            if (hit !== null)
                deepest = hit
        }
        return deepest
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
            compare(chips[0].height, 28)
            panel.width = 64
            panel.height = 200
            panel.panel = { edge: "left", rows: 2, applets: [
                root.spec("a", "start"), root.spec("b", "start")] }
            wait(20)
            chips = root.named(panel, "appletChip")
            compare(chips[0].y, chips[1].y)
            verify(chips[0].x + chips[0].width < chips[1].x)
            compare(chips[0].width, 28)
        }
        // AGENT-GUARD: the zone viewport's attached scroll bars are indicators
        // only. An unconditional, pointer-interactive bar spans the trailing
        // ~10 logical pixels of every zone even at zero opacity, which on a
        // stock 30px panel swallowed presses aimed at the bottom of a hosted
        // menu word or the edge of a status icon.
        function test_zoneScrollBarsNeverCoverHostedAppletHitAreas_data() {
            return [
                { tag: "horizontal", edge: "top", width: 480, height: 30 },
                { tag: "vertical", edge: "left", width: 30, height: 480 }
            ]
        }
        function test_zoneScrollBarsNeverCoverHostedAppletHitAreas(data) {
            panel.width = data.width
            panel.height = data.height
            panel.panel = { edge: data.edge, rows: 1, applets: [
                root.spec("start", "start"), root.spec("end", "end")] }
            wait(20)
            const chips = root.named(panel, "appletChip")
            compare(chips.length, 2)
            for (let chip of chips) {
                verify(chip.width > 0 && chip.height > 0)
                const probes = [
                    { tag: "bottom edge", x: chip.width / 2, y: chip.height - 1 },
                    { tag: "trailing corner", x: chip.width - 1, y: chip.height - 1 },
                    { tag: "leading bottom corner", x: 1, y: chip.height - 1 }
                ]
                for (let probe of probes) {
                    const point = chip.mapToItem(null, probe.x, probe.y)
                    const hit = root.deepestVisibleItemAt(panel, point.x, point.y)
                    verify(hit !== null)
                    verify(String(hit).indexOf("ScrollBar") === -1,
                           data.tag + " " + probe.tag + " is owned by " + hit)
                }
            }
        }

        // Overflow still reports itself: the bar appears on the overflowing
        // axis only, so the documented scroll affordance is not lost.
        function test_overflowingZoneStillShowsItsScrollBar() {
            panel.width = 400
            panel.height = 32
            const items = [root.spec("short", "start")]
            for (let i = 0; i < 20; ++i)
                items.push(root.spec("task" + i, "end"))
            panel.panel = { edge: "top", rows: 1, applets: items }
            wait(20)
            const end = findChild(panel, "panelZoneEnd")
            verify(end.contentWidth > end.width)
            const bars = root.named(end, "").filter(
                item => String(item).indexOf("ScrollBar") !== -1)
            const visibleBars = bars.filter(item => item.visible)
            compare(visibleBars.length, 1)
            verify(!visibleBars[0].interactive)
            const start = findChild(panel, "panelZoneStart")
            compare(root.named(start, "").filter(
                item => String(item).indexOf("ScrollBar") !== -1
                        && item.visible).length, 0)
        }

        function test_centeredDockPaintAndInputBoundsHugContent() {
            panel.width = 400
            panel.height = 80
            panel.panel = { id: "renamed-dock", edge: "bottom", alignment: "center",
                rows: 1, thickness: 80, applets: [
                    { id: "launcher", plugin: "launcher",
                      settings: { zone: "center", dockMode: true } },
                    root.spec("tasks", "center")] }
            // The task-list setting is what survives profile duplication; the
            // dock name itself is only a legacy presentation fallback.
            panel.panel.applets[1].settings.dockMode = true
            wait(20)
            const material = findChild(panel, "panelMaterial")
            verify(panel.dockMode)
            verify(material.width < panel.width)
            verify(panel.inputBounds.width < panel.width)
            verify(panel.inputBounds.height <= panel.height)
            verify(panel.inputBounds.width >= material.width)
            const initialMaterialWidth = material.width
            // More live task-list content grows the painted/input region; the
            // mask bridge observes inputBounds rather than assuming the
            // window's initial size is final.
            panel.panel = { id: "renamed-dock", edge: "bottom", alignment: "center",
                rows: 1, thickness: 80, applets: [
                    { id: "launcher", plugin: "launcher",
                      settings: { zone: "center", dockMode: true } },
                    root.spec("task-a", "center"), root.spec("task-b", "center"),
                    root.spec("task-c", "center")] }
            for (const applet of panel.panel.applets)
                applet.settings.dockMode = true
            wait(20)
            verify(material.width > initialMaterialWidth)
            verify(panel.inputBounds.width >= material.width)
        }
    }
}
