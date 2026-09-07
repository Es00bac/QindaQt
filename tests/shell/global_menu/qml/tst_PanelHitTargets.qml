// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import "../../../../src/shell/qml" as ShellComponents

// AGENT-NOTE: the production panel row hosts controls whose implicit height
// (24-28px) can exceed the cross-axis room a thin panel leaves after
// PanelContent's insets. This row proves the actually-hosted default 30px
// top panel, not an isolated component at a convenient size, keeps the
// global-menu word fully clickable including its lower edge.
Item {
    id: root
    width: 480
    height: 120

    property var theme: ({
        "cornerRadius": 6,
        "colors": {
            "surface": "#222624", "surfaceRaised": "#2c312e",
            "border": "#3c433f", "text": "#f2f1eb",
            "textMuted": "#a9afa9", "accent": "#8fc8b7",
            "accentText": "#10201b"
        }
    })

    QtObject {
        id: fakeGlobalMenuAccess
        property bool available: true
        property int activateCalls: 0
        property string lastId: ""
        property var items: [{
            "id": "file", "kind": "submenu", "text": "File",
            "enabled": true, "children": [{
                "id": "open", "kind": "action", "text": "Open",
                "enabled": true
            }]
        }]
        function activate(id) {
            ++activateCalls
            lastId = String(id)
        }
    }

    // Mirrors the default QindaQt profile's top panel: 30px thickness with
    // global-menu in the start zone.
    ShellComponents.PanelContent {
        id: panel
        objectName: "productionHitTargetPanel"
        width: 480
        height: 30
        theme: root.theme
        liveApplets: true
        globalMenuAppletAccess: fakeGlobalMenuAccess
        panel: ({
            "id": "command-bar", "edge": "top", "rows": 1, "alignment": "fill",
            "applets": [
                { "id": "menu", "plugin": "global-menu",
                  "settings": { "zone": "start" },
                  "runtime": { "ready": true, "entryPoint": "qindaqt.applets.global-menu" } }
            ]
        })
    }

    // Mirrors the window's own topmost-first pointer routing: the deepest
    // visible item whose bounds contain the scene point.
    function deepestVisibleItemAt(item, sceneX, sceneY) {
        const local = item.mapFromItem(null, sceneX, sceneY)
        if (local.x < 0 || local.y < 0 || local.x >= item.width || local.y >= item.height)
            return null
        let deepest = item
        for (const child of item.children || []) {
            if (!child.visible)
                continue
            const hit = deepestVisibleItemAt(child, sceneX, sceneY)
            if (hit !== null)
                deepest = hit
        }
        return deepest
    }

    TestCase {
        name: "PanelHitTargets"
        when: windowShown

        function init() {
            fakeGlobalMenuAccess.activateCalls = 0
            fakeGlobalMenuAccess.lastId = ""
            wait(20)
        }

        function test_stock30pxPanelRowFitsTheHostedControlHeight() {
            const chip = findChild(panel, "appletChip")
            verify(chip !== null)
            // The row must be at least as tall as the 24px control it hosts;
            // a shorter row forces a negative centering offset that pushes
            // part of the control's hit area outside the row.
            verify(chip.height >= 24)
        }

        // The zone viewport must not park an attached scroll bar over the
        // hosted control: a pointer-interactive overlay there consumes the
        // press before the applet ever sees it, however tall the control is.
        function test_noZoneScrollBarOverlaysTheHostedControl() {
            const top = findChild(panel, "globalMenuTopLevelItem")
            verify(top !== null)
            const probes = [
                { tag: "bottom edge", x: top.width / 2, y: top.height - 1 },
                { tag: "bottom-right corner", x: top.width - 1, y: top.height - 1 },
                { tag: "bottom-left corner", x: 1, y: top.height - 1 }
            ]
            for (const probe of probes) {
                const point = top.mapToItem(null, probe.x, probe.y)
                const hit = root.deepestVisibleItemAt(panel, point.x, point.y)
                verify(hit !== null)
                verify(String(hit).indexOf("ScrollBar") === -1,
                       probe.tag + " of the menu entry is owned by " + hit)
                // The entry itself must still claim the point it paints.
                verify(top.contains(Qt.point(probe.x, probe.y)),
                       probe.tag + " falls outside the menu entry's own bounds")
            }
        }

        function test_clickingTheLowerEdgeOfTheGlobalMenuWordOpensItsSubmenu() {
            const applet = findChild(panel, "globalMenuApplet")
            const top = findChild(panel, "globalMenuTopLevelItem")
            verify(applet !== null)
            verify(top !== null)
            verify(top.height >= 24)
            // One pixel above the bottom edge: the lower part of the
            // rendered "File" word, not its vertical center.
            mouseClick(top, top.width / 2, top.height - 1)
            const popup = applet.menuBar.menuAt(0)
            verify(popup !== null)
            tryCompare(popup, "opened", true)
            popup.dismiss()
            tryCompare(popup, "opened", false)
        }

        function test_clickingTheLowerRightCornerOfTheGlobalMenuWordOpensItsSubmenu() {
            const applet = findChild(panel, "globalMenuApplet")
            const top = findChild(panel, "globalMenuTopLevelItem")
            verify(top !== null)
            mouseClick(top, top.width - 1, top.height - 1)
            const popup = applet.menuBar.menuAt(0)
            verify(popup !== null)
            tryCompare(popup, "opened", true)
            popup.dismiss()
            tryCompare(popup, "opened", false)
        }
    }
}
