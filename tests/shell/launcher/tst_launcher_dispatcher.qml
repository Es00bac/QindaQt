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
    }
}
