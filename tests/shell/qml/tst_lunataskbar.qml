// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as Shell

// Worn Luna taskbar rendering (ADR-0124, "Luna taskbar rendering"). The
// bliss-taskbar panel id hosts applets directly on the Luna bar, paints the
// notification-area well behind the end zone, and hands the Luna dressing to
// every hosted applet; a copied or renamed panel keeps the ordinary chips.
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

    // No applet here opts in through `presentation`: the panel id alone must
    // select every Luna path.
    function lunaPanel(id) {
        return { id: id, edge: "bottom", alignment: "fill", rows: 1, thickness: 40,
            applets: [
                { id: "start", plugin: "start-menu", settings: { zone: "start" },
                  runtime: { ready: true, entryPoint: "qindaqt.applets.start-menu" } },
                { id: "notifications", plugin: "notification-center",
                  settings: { zone: "end" },
                  runtime: { ready: true,
                             entryPoint: "qindaqt.applets.notification-center" } },
                { id: "clock", plugin: "clock", settings: { zone: "end" },
                  runtime: { ready: true, entryPoint: "qindaqt.applets.clock" } }
            ] }
    }

    Shell.PanelContent {
        id: panel
        width: 640
        height: 40
        theme: ({ colors: {} })
        liveApplets: true
        panel: root.lunaPanel("bliss-taskbar")
    }

    TestCase {
        name: "LunaTaskbar"
        when: windowShown

        function init() {
            panel.panel = root.lunaPanel("bliss-taskbar")
            wait(20)
        }

        function test_appletsSitOnTheBarWithoutChips() {
            verify(panel.lunaMode)
            const chips = root.named(panel, "appletChip")
            compare(chips.length, 3)
            for (let chip of chips) {
                verify(chip.lunaMode)
                compare(chip.color.a, 0)
            }
        }

        function test_trayWellSpansTheEndZoneAtFullHeight() {
            const well = findChild(panel, "lunaTrayWell")
            const end = findChild(panel, "panelZoneEnd")
            verify(well.visible)
            verify(well.x < end.x)
            compare(well.x + well.width, panel.width)
            compare(well.y, 0)
            compare(well.height, panel.height)
        }

        function test_hostedAppletsTakeTheLunaPath() {
            const clock = findChild(panel, "clockApplet")
            verify(clock.luna)
            const label = findChild(clock, "clockAppletLabel")
            compare(label.font.family, "Tahoma")
            verify(!label.font.bold)
            compare(label.style, Text.Normal)
            verify(Qt.colorEqual(label.color, "white"))
            const bell = findChild(panel, "notificationCenterAppletIcon")
            verify(bell.visible)
            compare(bell.name, "notifications")
            verify(!findChild(panel, "notificationCenterAppletGlyph").visible)
        }

        function test_renamedPanelKeepsTheStandardPresentation() {
            panel.panel = root.lunaPanel("copied-taskbar")
            wait(20)
            verify(!panel.lunaMode)
            verify(!findChild(panel, "lunaTrayWell").visible)
            const chips = root.named(panel, "appletChip")
            compare(chips.length, 3)
            for (let chip of chips) {
                verify(!chip.lunaMode)
                verify(chip.color.a > 0)
            }
            verify(!findChild(panel, "clockApplet").luna)
            verify(!findChild(panel, "notificationCenterAppletIcon").visible)
            verify(findChild(panel, "notificationCenterAppletGlyph").visible)
        }
    }
}
