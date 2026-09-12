// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as Shell

Item {
    id: root
    width: 900
    height: 500

    QtObject {
        id: fakePanelConfig
        property var store: ({})
        property int openCalls: 0
        signal panelSettingsChanged()
        function panelSettings(panelId) { return store[panelId] ?? {} }
        function setPanelSetting(panelId, key, value) {
            const entry = Object.assign({}, store[panelId] ?? {})
            entry[key] = value
            const next = Object.assign({}, store)
            next[panelId] = entry
            store = next
            panelSettingsChanged()
            return true
        }
        function openCustomize() { openCalls++; return true }
    }

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
        width: 400
        height: 80
        theme: ({ colors: {} })
        liveApplets: true
        panel: ({ edge: "top", rows: 1, applets: [] })
    }

    TestCase {
        name: "DockGeometry"
        when: windowShown

        function init() {
            panel.width = 400
            panel.height = 80
            panel.panelQuickConfig = null
            panel.launcherAppletAccess = null
            fakePanelConfig.store = ({})
            fakePanelConfig.openCalls = 0
        }

        function test_paintAndInputBoundsHugContent() {
            panel.panel = { id: "renamed-dock", edge: "bottom", alignment: "center",
                rows: 1, thickness: 80, applets: [
                    { id: "launcher", plugin: "launcher",
                      settings: { zone: "center", dockMode: true } },
                    root.spec("tasks", "center")] }
            wait(20)
            const material = findChild(panel, "panelMaterial")
            verify(panel.dockMode)
            verify(material.width < panel.width)
            verify(panel.inputBounds.width < panel.width)
            verify(panel.inputBounds.height <= panel.height)
            verify(panel.inputBounds.width >= material.width)
            verify(panel.inputBounds.y < material.y)
            fuzzyCompare(panel.inputBounds.y + panel.inputBounds.height,
                         panel.height, 0.01)
            panel.panelQuickConfig = fakePanelConfig
            fakePanelConfig.store = { "renamed-dock": { dockZoom: false } }
            fakePanelConfig.panelSettingsChanged()
            tryVerify(() => panel.inputBounds.y === material.y)
            compare(panel.inputBounds.height, material.height)
            verify(panel.applyPanelSetting("dockZoom", true))
            const initialMaterialWidth = material.width
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

        function test_tilesShrinkWithoutChangingPreference() {
            panel.width = 240
            panel.launcherAppletAccess = ({})
            panel.panelQuickConfig = fakePanelConfig
            fakePanelConfig.store = { "fit-dock": { dockTileSize: 64 } }
            fakePanelConfig.panelSettingsChanged()
            const applets = []
            for (let i = 0; i < 5; ++i) {
                applets.push({ id: "launcher-" + i, plugin: "launcher",
                    settings: { zone: "center", dockMode: true },
                    runtime: { ready: true,
                               entryPoint: "qindaqt.applets.launcher" } })
            }
            panel.panel = { id: "fit-dock", edge: "bottom",
                alignment: "center", rows: 1, thickness: 80, applets: applets }
            wait(20)
            const center = findChild(panel, "panelZoneCenter")
            compare(panel.dockTileSize, 64)
            verify(panel.effectiveDockTileSize < panel.dockTileSize)
            verify(panel.effectiveDockTileSize >= center.minimumFittedDockTileSize)
            const fittedSize = panel.effectiveDockTileSize
            verify(center.contentWidth <= center.width + 0.01)

            panel.width = 600
            tryCompare(panel, "effectiveDockTileSize", 64)
            compare(fakePanelConfig.store["fit-dock"].dockTileSize, 64)
            verify(panel.effectiveDockTileSize > fittedSize)

            for (let i = 5; i < 20; ++i) {
                applets.push({ id: "launcher-" + i, plugin: "launcher",
                    settings: { zone: "center", dockMode: true },
                    runtime: { ready: true,
                               entryPoint: "qindaqt.applets.launcher" } })
            }
            panel.width = 160
            panel.panel = { id: "fit-dock", edge: "bottom",
                alignment: "center", rows: 1, thickness: 80, applets: applets }
            wait(20)
            compare(panel.effectiveDockTileSize, center.minimumFittedDockTileSize)
            verify(panel.dockOverflowFallback)
            verify(center.contentWidth > center.width)
            compare(panel.dockTileSize, 64)
        }

        function test_fullWidthDockScrollsAtFitFloor() {
            const applets = [{ id: "launcher", plugin: "launcher",
                settings: { zone: "center", dockMode: true } }]
            applets.push(root.spec("task-a", "center"), root.spec("task-b", "center"))
            panel.panel = { id: "d", edge: "bottom", alignment: "center",
                rows: 1, thickness: 80, applets: applets }
            for (const applet of panel.panel.applets)
                applet.settings.dockMode = true
            wait(20)
            const material = findChild(panel, "panelMaterial")
            const center = findChild(panel, "panelZoneCenter")
            verify(material.width < panel.width)
            verify(center.contentWidth <= center.width + 0.01)

            for (let i = 0; i < 40; ++i)
                applets.push(root.spec("overflow" + i, "center"))
            panel.panel = { id: "d", edge: "bottom", alignment: "center",
                rows: 1, thickness: 80, applets: applets }
            for (const applet of panel.panel.applets)
                applet.settings.dockMode = true
            wait(20)
            verify(material.width >= panel.width - 1)
            verify(center.contentWidth > center.width)
            const bars = root.named(center, "panelZoneHorizontalOverflowBar")
            compare(bars.length, 1)
            verify(bars[0].visible)
            verify(!bars[0].interactive)
            center.contentX = center.contentWidth - center.width
            tryVerify(() => center.contentX > 0)
        }

        function test_quickSettingsPersistIntegerSize() {
            panel.panelQuickConfig = fakePanelConfig
            panel.panel = { id: "test-dock", edge: "bottom", alignment: "center",
                rows: 1, thickness: 80,
                applets: [{ id: "tasks", plugin: "task-list",
                    settings: { zone: "center", dockMode: true },
                    runtime: { ready: true,
                               entryPoint: "qindaqt.applets.task-list" } }] }
            wait(20)
            compare(panel.dockTileSize, 60)
            verify(panel.applyPanelSetting("dockTileSize", 37))
            tryCompare(panel, "dockTileSize", 37)
            compare(fakePanelConfig.store["test-dock"].dockTileSize, 37)
            verify(panel.applyPanelSetting("transparency", false))
            tryCompare(panel, "materialTranslucent", false)

            const menu = findChild(panel, "panelConfigMenu")
            menu.popup()
            tryVerify(() => menu.opened)
            const slider = findChild(panel, "panelConfigTileSizeSlider")
            const input = findChild(panel, "panelConfigTileSizeInput")
            compare(slider.from, 32); compare(slider.to, 64)
            compare(slider.stepSize, 1); compare(slider.value, 37)
            compare(input.from, 32); compare(input.to, 64)
            compare(input.stepSize, 1); compare(input.value, 37)
            menu.close()

            panel.panel = { id: "top-bar", edge: "top", rows: 1, applets: [] }
            wait(20)
            menu.popup()
            tryVerify(() => menu.opened)
            verify(!findChild(panel, "panelConfigDockZoom").visible)
            verify(!findChild(panel, "panelConfigTileSize").visible)
            menu.close()
        }
    }
}
