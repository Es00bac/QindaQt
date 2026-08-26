// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../src/shell/qml" as ShellComponents

Item {
    width: 180
    height: 90

    ShellComponents.DecorationButtons {
        id: controls

        x: 20
        y: 20
        width: implicitWidth
        height: implicitHeight
        theme: ({
            "colors": {"textMuted": "#60716c"},
            "decoration": {
                "buttonStyle": "traffic-lights",
                "hoverGlyphs": true,
                "closeColor": "#ff5f57",
                "minimizeColor": "#febc2e",
                "maximizeColor": "#28c840"
            }
        })
    }

    TestCase {
        name: "DecorationButtons"
        when: windowShown

        function test_trafficLightGlyphsFollowHover() {
            verify(!controls.glyphsVisible)
            mouseMove(controls, controls.width / 2, controls.height / 2)
            tryCompare(controls, "glyphsVisible", true)
            mouseMove(controls, controls.width + 40, controls.height + 40)
            tryCompare(controls, "glyphsVisible", false)
        }
    }
}
