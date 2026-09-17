// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as Shell

// Panel zone budget (ADR-0188): a separate failure model from
// tst_panelgeometry.qml. Those rows prove that zones stay disjoint, scroll
// their own overflow and honour `rows`; these prove *how much* extent each
// zone is given, which is the rule the global menu depends on.
Item {
    id: root
    width: 900
    height: 500

    function spec(id, zone) {
        return { id: id, plugin: "task-list", settings: { zone: zone },
            runtime: { ready: true, entryPoint: "qindaqt.applets.task-list" } }
    }
    // Same shape plus the declared manifest minimum the applet resolver
    // republishes as `runtime.mainAxisMinimum`. The zone budget reserves the
    // sum of these for the zones that yield.
    function specWithMinimum(id, zone, minimum) {
        return { id: id, plugin: "task-list", settings: { zone: zone },
            runtime: { ready: true, entryPoint: "qindaqt.applets.task-list",
                mainAxisMinimum: minimum } }
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
        name: "PanelZoneBudget"
        when: windowShown
        function init() {
            panel.width = 160
            panel.height = 32
            panel.panel = { edge: "top", rows: 1, applets: [] }
            wait(20)
        }
        // O4: the start zone hosts the active application's menu bar, whose
        // desire is genuinely unbounded. The old max-min split capped it at a
        // third of the panel; it is now served first and bounded only by what
        // its neighbours have declared they need.
        function test_startZoneIsServedBeforeItsNeighbours() {
            panel.width = 1920
            panel.panel = { edge: "top", rows: 1, applets: [
                root.specWithMinimum("menu", "start", 160),
                root.specWithMinimum("tasks", "center", 120),
                root.specWithMinimum("clock", "end", 56)] }
            wait(20)
            const start = findChild(panel, "panelZoneStart")
            const center = findChild(panel, "panelZoneCenter")
            const end = findChild(panel, "panelZoneEnd")
            // Every zone here wants far less than a third, so all are satisfied.
            compare(start.width, start.desiredExtent)
            compare(end.width, end.desiredExtent)
            compare(center.width, center.desiredExtent)
            // Disjoint and inside the content box, in order.
            verify(start.x + start.width <= center.x + 0.01)
            verify(center.x + center.width <= end.x + 0.01)
            verify(end.x + end.width <= panel.width - 4 + 0.01)
        }
        function test_greedyStartZoneTakesEverythingNeighbourMinimumsAllow() {
            panel.width = 600
            // Twelve start applets want far more than the panel; the two
            // neighbours declare 120 and 56.
            const items = [root.specWithMinimum("tasks", "center", 120),
                           root.specWithMinimum("clock", "end", 56)]
            for (let i = 0; i < 12; ++i)
                items.push(root.specWithMinimum("menu" + i, "start", 20))
            panel.panel = { edge: "top", rows: 1, applets: items }
            wait(20)
            const start = findChild(panel, "panelZoneStart")
            const center = findChild(panel, "panelZoneCenter")
            const end = findChild(panel, "panelZoneEnd")
            const extent = panel.width - 8
            // The old rule would have given the start zone at most extent / 3.
            verify(start.width > extent / 3)
            // It takes exactly what the declared neighbour minimums leave.
            const centerMinimum = Math.min(center.desiredExtent, 120)
            const endMinimum = Math.min(end.desiredExtent, 56)
            compare(start.width, Math.min(start.desiredExtent,
                                          extent - centerMinimum - endMinimum))
            // AGENT-GUARD: the yielding zones keep their declared minimums and
            // the three budgets never exceed the content box.
            verify(end.width >= endMinimum - 0.01)
            verify(center.width >= centerMinimum - 0.01)
            verify(start.width + center.width + end.width <= extent + 0.01)
            verify(start.x + start.width <= center.x + 0.01)
            verify(center.x + center.width <= end.x + 0.01)
            verify(end.x + end.width <= panel.width - 4 + 0.01)
            // The center zone has moved past the start zone's right edge.
            verify(center.x >= start.x + start.width - 0.01)
        }
        function test_zonesWithoutDeclaredMinimumsStillYieldToTheStartZone() {
            panel.width = 600
            const items = [root.spec("tasks", "center"), root.spec("clock", "end")]
            for (let i = 0; i < 12; ++i)
                items.push(root.spec("menu" + i, "start"))
            panel.panel = { edge: "top", rows: 1, applets: items }
            wait(20)
            const start = findChild(panel, "panelZoneStart")
            const center = findChild(panel, "panelZoneCenter")
            const end = findChild(panel, "panelZoneEnd")
            const extent = panel.width - 8
            // A manifest that declares no minimum reserves nothing, so the
            // start zone may take the whole content box.
            compare(start.width, Math.min(start.desiredExtent, extent))
            verify(start.width + center.width + end.width <= extent + 0.01)
            verify(start.x + start.width <= center.x + 0.01)
            verify(center.x + center.width <= end.x + 0.01)
        }
        function test_panelNarrowerThanEveryMinimumStaysInsideItsBox() {
            panel.width = 120
            panel.panel = { edge: "top", rows: 1, applets: [
                root.specWithMinimum("menu", "start", 400),
                root.specWithMinimum("tasks", "center", 300),
                root.specWithMinimum("clock", "end", 200)] }
            wait(20)
            const start = findChild(panel, "panelZoneStart")
            const center = findChild(panel, "panelZoneCenter")
            const end = findChild(panel, "panelZoneEnd")
            const extent = panel.width - 8
            // Nothing is negative, nothing overlaps, nothing leaves the box.
            verify(start.width >= 0)
            verify(center.width >= 0)
            verify(end.width >= 0)
            verify(start.width + center.width + end.width <= extent + 0.01)
            verify(start.x + start.width <= center.x + 0.01)
            verify(center.x + center.width <= end.x + 0.01)
            verify(end.x + end.width <= panel.width - 4 + 0.01)
        }
        function test_startZoneBudgetHoldsAtEveryStockResolution() {
            const widths = [1366, 1920, 2560]
            for (const width of widths) {
                panel.width = width
                const items = [root.specWithMinimum("tasks", "center", 48),
                               root.specWithMinimum("clock", "end", 332)]
                for (let i = 0; i < 24; ++i)
                    items.push(root.specWithMinimum("menu" + i, "start", 20))
                panel.panel = { edge: "top", rows: 1, applets: items }
                wait(20)
                const start = findChild(panel, "panelZoneStart")
                const center = findChild(panel, "panelZoneCenter")
                const end = findChild(panel, "panelZoneEnd")
                const extent = width - 8
                verify(start.width > extent / 3)
                verify(start.width + center.width + end.width <= extent + 0.01)
                verify(start.x + start.width <= center.x + 0.01)
                verify(center.x + center.width <= end.x + 0.01)
                verify(end.x + end.width <= width - 4 + 0.01)
            }
            panel.width = 160
        }
        function test_verticalPanelKeepsTheBalancedSplit() {
            panel.width = 64
            panel.height = 200
            // All three zones greedy: this is the only configuration where the
            // two rules disagree. The max-min split caps each at an equal
            // share; priority order would hand the start zone everything.
            const items = []
            for (const zone of ["start", "center", "end"])
                for (let i = 0; i < 8; ++i)
                    items.push(root.specWithMinimum(zone + i, zone, 20))
            panel.panel = { edge: "left", rows: 1, applets: items }
            wait(20)
            const start = findChild(panel, "panelZoneStart")
            const center = findChild(panel, "panelZoneCenter")
            const end = findChild(panel, "panelZoneEnd")
            const extent = panel.height - 8
            verify(start.desiredExtent > extent)
            verify(center.desiredExtent > extent)
            verify(end.desiredExtent > extent)
            // A vertical panel has no reading order to prefer, so it keeps the
            // equal share it always had.
            verify(start.height <= extent / 3 + 0.01)
            verify(center.height <= extent / 3 + 0.01)
            verify(end.height <= extent / 3 + 0.01)
            verify(start.height + center.height + end.height <= extent + 0.01)
            verify(start.y + start.height <= center.y + 0.01)
            verify(center.y + center.height <= end.y + 0.01)
            panel.width = 160
            panel.height = 32
        }
    }
}
