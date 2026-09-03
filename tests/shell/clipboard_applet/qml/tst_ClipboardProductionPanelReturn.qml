// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest
import QindaQt.Shell.ClipboardApplet 1.0
import QindaQt.Shell.ClipboardApplet.Tests 1.0 as Harness

Item {
    id: root
    width: 180
    height: 60

    readonly property bool harnessReady: Harness.Harness.ready

    FakeClipboardController { id: fakeController }

    ClipboardPanelApplet {
        id: applet
        width: 160
        height: 40
        controller: fakeController
        theme: ({
            "cornerRadius": 6,
            "colors": {
                "surface": "#222624", "surfaceRaised": "#2c312e",
                "border": "#3c433f", "text": "#f2f1eb",
                "textMuted": "#a9afa9", "accent": "#8fc8b7"
            }
        })
    }

    TestCase {
        name: "ClipboardProductionPanelReturn"
        when: windowShown && root.harnessReady

        function test_returnActivatesSummary() {
            const summary = findChild(applet, "clipboardPanelSummary")
            const popup = findChild(applet, "clipboardPanelPopup")
            verify(summary !== null)
            verify(popup !== null)
            summary.forceActiveFocus(Qt.TabFocusReason)
            tryVerify(function() { return summary.activeFocus })
            keyClick(Qt.Key_Return)
            tryCompare(popup, "opened", true)
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
        }
    }
}
