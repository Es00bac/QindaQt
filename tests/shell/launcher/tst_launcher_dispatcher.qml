// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as ShellComponents

Item {
    id: testRoot
    width: 480
    height: 160

    property var theme: ({
        "cornerRadius": 8,
        "colors": {
            "surface": "#222624",
            "surfaceRaised": "#2c312e",
            "text": "#f2f1eb",
            "accent": "#8fc8b7"
        }
    })

    QtObject {
        id: fakeLauncherAccess
        property string phase: "ready"
    }

    function launcherApplet(ready) {
        return {
            "id": "applications",
            "plugin": "launcher",
            "settings": { "zone": "start" },
            "runtime": {
                "ready": ready,
                "entryPoint": "qindaqt.applets.launcher"
            }
        }
    }

    Component {
        id: rowComponent
        ShellComponents.PanelAppletRow {
            width: 240
            height: 40
            panel: ({ "applets": [testRoot.launcherApplet(true)] })
            theme: testRoot.theme
            zone: "start"
            liveApplets: true
            launcherAppletAccess: fakeLauncherAccess
        }
    }

    Component {
        id: thinStockRowComponent
        ShellComponents.PanelAppletRow {
            // minimal.json is 26 px with 4 px panel padding on each side.
            width: 240
            height: 18
            panel: ({ "applets": [testRoot.launcherApplet(true)] })
            theme: testRoot.theme
            zone: "start"
            liveApplets: true
            launcherAppletAccess: fakeLauncherAccess
        }
    }

    Component {
        id: previewChipComponent
        ShellComponents.AppletChip {
            width: 100
            height: 40
            applet: ({
                "id": "applications",
                "plugin": "launcher",
                "settings": {}
            })
            theme: testRoot.theme
            liveApplets: false
        }
    }

    TestCase {
        name: "LauncherPanelDispatcher"
        when: windowShown

        function test_productionRowCarriesOnlyLauncherFacade() {
            const row = createTemporaryObject(rowComponent, testRoot)
            verify(row !== null)
            const launcher = findChild(row, "launcherApplet")
            verify(launcher !== null)
            verify(launcher.visible)
            compare(launcher.access, fakeLauncherAccess)
            verify(launcher.enabled)
        }

        function test_previewRendersCompiledDisabledFallback() {
            const chip = createTemporaryObject(previewChipComponent, testRoot)
            verify(chip !== null)
            verify(chip.usesLiveContent)
            const launcher = findChild(chip, "launcherApplet")
            verify(launcher !== null)
            verify(launcher.visible)
            compare(launcher.access, null)
            verify(!launcher.enabled)
        }

        function test_thinnestStockPanelDoesNotClipLiveIcon() {
            const row = createTemporaryObject(thinStockRowComponent, testRoot)
            verify(row !== null)
            const chip = findChild(row, "appletChip")
            const launcher = findChild(row, "launcherApplet")
            const icon = findChild(row, "launcherAppletIcon")
            verify(chip !== null)
            verify(launcher !== null)
            verify(icon !== null)
            compare(chip.height, 18)
            verify(!chip.clip)
            compare(launcher.height, chip.height)
            compare(launcher.summaryIconExtent, 14)
            compare(icon.height, launcher.summaryIconExtent)
            verify(icon.y >= 0)
            verify(icon.y + icon.height <= launcher.height)
        }
    }
}
