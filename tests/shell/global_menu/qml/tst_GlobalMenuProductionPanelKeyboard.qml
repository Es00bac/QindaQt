// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import "../../../../src/shell/qml" as ShellComponents

Item {
    id: root
    width: 520
    height: 180

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
        id: fakeAccess
        property bool available: true
        property string phase: "ready"
        property int activateCalls: 0
        property string lastId: ""
        property var items: [{
            "id": "file", "kind": "submenu", "text": "File",
            "enabled": true, "children": [{
                "id": "open", "kind": "action", "text": "Open",
                "enabled": true
            }, {
                "id": "recent", "kind": "submenu", "text": "Recent",
                "enabled": true, "children": [{
                    "id": "alpha", "kind": "action", "text": "Alpha",
                    "enabled": true
                }]
            }]
        }]
        function activate(id) {
            ++activateCalls
            lastId = String(id)
        }
    }

    TextInput {
        id: beforePanel
        objectName: "beforeGlobalMenuPanel"
        text: "before"
        activeFocusOnTab: true
    }

    ShellComponents.PanelAppletRow {
        id: panelRow
        objectName: "productionGlobalMenuPanelRow"
        anchors.left: parent.left
        anchors.top: beforePanel.bottom
        width: 480
        height: 40
        zone: "start"
        liveApplets: true
        theme: root.theme
        globalMenuAppletAccess: fakeAccess
        panel: ({
            "applets": [{
                "id": "application-menu",
                "plugin": "global-menu",
                "settings": { "zone": "start" },
                "runtime": {
                    "ready": true,
                    "entryPoint": "qindaqt.applets.global-menu"
                }
            }]
        })
    }

    TestCase {
        name: "GlobalMenuProductionPanelKeyboard"
        when: windowShown

        function init() {
            fakeAccess.activateCalls = 0
            fakeAccess.lastId = ""
        }

        function test_keysReachPanelTraverseActivateAndClose() {
            // AGENT-NOTE: P1-01 requires the real BuiltinAppletContent/
            // PanelAppletRow route and an independent popup window; the old
            // isolated applet test passed while RuntimePanel rejected focus.
            const applet = findChild(panelRow, "globalMenuApplet")
            const top = findChild(panelRow, "globalMenuTopLevelItem")
            verify(applet !== null)
            verify(top !== null)

            beforePanel.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Tab)
            tryVerify(function() { return applet.menuBar.activeFocus })
            keyClick(Qt.Key_Down)
            const bar = applet.menuBar
            verify(bar !== null)
            const popup = bar.menuAt(0)
            verify(popup !== null)
            compare(popup.popupType, Popup.Window)
            tryCompare(popup, "opened", true)
            tryCompare(popup, "currentIndex", 0)
            keyClick(Qt.Key_Down)
            compare(popup.currentIndex, 1)
            keyClick(Qt.Key_Right)
            const recent = popup.menuAt(1)
            tryCompare(recent, "opened", true)
            tryCompare(recent, "currentIndex", 0)
            keyClick(Qt.Key_Space)
            compare(fakeAccess.activateCalls, 1)
            compare(fakeAccess.lastId, "alpha")
            tryCompare(popup, "opened", false)
        }
    }
}
