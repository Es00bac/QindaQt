// SPDX-License-Identifier: GPL-3.0-or-later
import QindaQt.Shell.GlobalMenu as GlobalMenuComponents
import QtQuick
import QtQuick.Controls
import QtTest

Item {
    id: root
    width: 640
    height: 400

    property var theme: ({
        "colors": {
            "text": "#f2f1eb", "textMuted": "#a9afa4",
            "surfaceRaised": "#2c312e", "border": "#3c433f",
            "accent": "#8fc8b7", "accentText": "#10201b"
        },
        "cornerRadius": 6
    })

    QtObject {
        id: fakeAccess
        property bool available: true
        property var items: []
        property int activateCalls: 0
        property string lastId: ""
        function activate(id) {
            ++activateCalls
            lastId = String(id)
        }
    }

    GlobalMenuComponents.GlobalMenuApplet {
        id: applet
        anchors.top: parent.top
        width: 420
        height: 40
        access: fakeAccess
        theme: root.theme
    }

    TestCase {
        name: "GlobalMenuNativeSwitch"
        when: windowShown

        function menuItems() {
            return [{
                "id": "file", "kind": "submenu", "text": "File",
                "enabled": true, "children": [{
                    "id": "new", "kind": "action", "text": "New",
                    "enabled": true
                }]
            }, {
                "id": "edit", "kind": "submenu", "text": "Edit",
                "enabled": true, "children": [{
                    "id": "undo", "kind": "action", "text": "Undo",
                    "enabled": true
                }]
            }]
        }

        function init() {
            applet.visible = true
            fakeAccess.items = menuItems()
            fakeAccess.activateCalls = 0
            fakeAccess.lastId = ""
            const bar = menuBar()
            for (let i = 0; i < bar.count; ++i)
                bar.menuAt(i).dismiss()
        }

        function menuBar() {
            const bar = applet.menuBar
            verify(bar !== null)
            compare(bar.count, 2)
            return bar
        }

        function test_menuBarLabelsFitActualContent() {
            const bar = menuBar()
            for (let index = 0; index < bar.count; ++index) {
                const label = bar.itemAt(index).contentItem
                compare(label.text, fakeAccess.items[index].text)
                verify(!label.truncated)
            }
        }

        function test_pressOtherEntrySwitchesBeforeRelease() {
            const bar = menuBar()
            mouseClick(bar.itemAt(1))
            tryCompare(bar.menuAt(1), "opened", true)

            // This is the production failure boundary: the old popup owns a
            // native grab while the press lands on File. Qt's MenuBar changes
            // currentItem and opens File during the press, without waiting
            // for the release that the prior popup may consume.
            mousePress(bar.itemAt(0))
            tryCompare(bar.menuAt(0), "opened", true)
            compare(bar.menuAt(1).opened, false)
            compare(fakeAccess.activateCalls, 0)
            mouseRelease(bar.itemAt(0))
            compare(bar.menuAt(0).opened, true)
            compare(fakeAccess.activateCalls, 0)
        }

        function test_hoverSwitchesOnlyWhileMenuOpen() {
            const bar = menuBar()
            mouseMove(root, 2, root.height - 2)
            mouseMove(bar.itemAt(0), bar.itemAt(0).width / 2,
                      bar.itemAt(0).height / 2)
            compare(bar.menuAt(0).opened, false)

            mouseClick(bar.itemAt(1))
            tryCompare(bar.menuAt(1), "opened", true)
            mouseMove(root, 2, root.height - 2)
            mouseMove(bar.itemAt(0), bar.itemAt(0).width / 2,
                      bar.itemAt(0).height / 2)
            tryCompare(bar.menuAt(0), "opened", true)
            compare(bar.menuAt(1).opened, false)
        }

        function test_firstPopupActionDispatchesExactlyOnce() {
            const bar = menuBar()
            const fileMenu = bar.menuAt(0)
            mouseClick(bar.itemAt(0))
            tryCompare(fileMenu, "opened", true)
            compare(fileMenu.count, 1)
            mouseClick(fileMenu.itemAt(0))
            compare(fakeAccess.activateCalls, 1)
            compare(fakeAccess.lastId, "new")
            tryCompare(fileMenu, "opened", false)
        }

    }
}
