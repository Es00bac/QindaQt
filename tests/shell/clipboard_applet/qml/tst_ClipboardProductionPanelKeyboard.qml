// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtTest
import QindaQt.Shell.ClipboardApplet.Tests 1.0 as Harness
import "../../../../src/shell/qml" as ShellComponents

Item {
    id: root
    width: 560
    height: 220

    readonly property bool harnessReady: Harness.Harness.ready

    property var theme: ({
        "cornerRadius": 6,
        "colors": {
            "surface": "#222624", "surfaceRaised": "#2c312e",
            "border": "#3c433f", "text": "#f2f1eb",
            "textMuted": "#a9afa9", "accent": "#8fc8b7"
        }
    })

    FakeClipboardController { id: fakeController }

    TextInput {
        id: beforePanel
        objectName: "beforeClipboardPanel"
        text: "before"
        activeFocusOnTab: true
    }

    ShellComponents.PanelAppletRow {
        id: panelRow
        objectName: "productionClipboardPanelRow"
        anchors.left: parent.left
        anchors.top: beforePanel.bottom
        width: 520
        height: 40
        zone: "end"
        liveApplets: true
        theme: root.theme
        clipboardAppletAccess: fakeController
        panel: ({
            "applets": [{
                "id": "clipboard", "plugin": "clipboard",
                "settings": { "zone": "end" },
                "runtime": {
                    "ready": true,
                    "entryPoint": "qindaqt.applets.clipboard"
                }
            }]
        })
    }

    TestCase {
        name: "ClipboardProductionPanelKeyboard"
        when: windowShown && root.harnessReady

        function test_a_summaryOpensWindowPopupTraversesAndEscapeCloses() {
            const applet = findChild(panelRow, "clipboardPanelApplet")
            const summary = findChild(panelRow, "clipboardPanelSummary")
            verify(applet !== null)
            verify(summary !== null)
            compare(summary.Accessible.role, Accessible.Button)
            compare(summary.Accessible.name, "Clipboard history")
            compare(summary.text, "")
            verify(summary.width <= panelRow.height)
            const icon = findChild(summary, "clipboardPanelIcon")
            verify(icon !== null)
            compare(icon.name, "edit-paste")
            verify(icon.resolved)
            const iconImage = findChild(icon, "iconImage")
            verify(iconImage !== null)
            verify(iconImage.source.toString().indexOf("edit-paste") !== -1)
            verify(icon.color.a > 0)
            verify(!summary.Accessible.checkable)

            beforePanel.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Tab)
            tryVerify(function() { return summary.activeFocus })
            keyClick(Qt.Key_Space)

            const popup = findChild(applet, "clipboardPanelPopup")
            verify(popup !== null)
            compare(popup.popupType, Popup.Window)
            tryCompare(popup, "opened", true)

            const search = findChild(popup.contentItem, "clipboardSearchField")
            verify(search !== null)
            tryVerify(function() { return search.activeFocus })
            keyClick(Qt.Key_Tab)
            tryVerify(function() { return !search.activeFocus })
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
        }

    }
}
