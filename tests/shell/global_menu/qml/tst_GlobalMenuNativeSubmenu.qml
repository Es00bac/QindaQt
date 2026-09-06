// SPDX-License-Identifier: GPL-3.0-or-later
import QindaQt.Shell.GlobalMenu as GlobalMenuComponents
import QtQuick
import QtQuick.Controls
import QtTest

Item {
    id: root
    width: 480
    height: 400

    property var theme: ({
        "colors": {
            "text": "#f2f1eb", "textMuted": "#a9afa9",
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

    Component {
        id: appletComponent
        GlobalMenuComponents.GlobalMenuApplet {
            width: 420
            height: 28
            access: fakeAccess
            theme: root.theme
        }
    }

    TestCase {
        name: "GlobalMenuNativeSubmenu"
        when: windowShown

        function menuItems() {
            return [{
                "id": "file", "kind": "submenu", "text": "File",
                "enabled": true, "children": [{
                    "id": "open", "kind": "action", "text": "Open",
                    "enabled": true, "shortcutText": "Ctrl+O"
                }, {
                    "id": "recent", "kind": "submenu", "text": "Recent",
                    "enabled": true, "children": [{
                        "id": "alpha", "kind": "action", "text": "Alpha",
                        "enabled": true, "checkable": true, "checked": true
                    }]
                }, {
                    "id": "separator", "kind": "separator", "enabled": false
                }, {
                    "id": "disabled", "kind": "action", "text": "Disabled",
                    "enabled": false
                }]
            }]
        }

        function init() {
            fakeAccess.available = true
            fakeAccess.items = menuItems()
            fakeAccess.activateCalls = 0
            fakeAccess.lastId = ""
        }

        function nativeTree(applet) {
            const bar = applet.menuBar
            verify(bar !== null)
            tryCompare(bar, "count", 1)
            const fileMenu = bar.menuAt(0)
            tryCompare(fileMenu, "count", 4)
            compare(fileMenu.menuAt(1).title, "Recent")
            return {"bar": bar, "file": fileMenu,
                    "recent": fileMenu.menuAt(1)}
        }

        function test_projectedTreePreservesActionsSeparatorsAndState() {
            const applet = createTemporaryObject(appletComponent, root)
            const tree = nativeTree(applet)
            compare(tree.file.itemAt(0).text, "Open")
            compare(tree.file.itemAt(0).Accessible.description, "Ctrl+O")
            compare(tree.file.itemAt(2).objectName, "globalMenuNativeSeparator")
            verify(!tree.file.itemAt(3).enabled)
            compare(tree.recent.count, 1)
            verify(!tree.recent.itemAt(0).checkable)
            verify(tree.recent.itemAt(0).Accessible.checkable)
            verify(tree.recent.itemAt(0).Accessible.checked)
        }

        function test_keyboardTraversalActivatesExactlyOnce() {
            const applet = createTemporaryObject(appletComponent, root)
            const tree = nativeTree(applet)
            const top = tree.bar.itemAt(0)
            root.Window.window.requestActivate()
            tryCompare(root.Window.window, "active", true)
            top.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Down)
            tryCompare(tree.file, "opened", true)
            tryCompare(tree.file, "currentIndex", 0)
            keyClick(Qt.Key_Down)
            compare(tree.file.currentIndex, 1)
            keyClick(Qt.Key_Right)
            tryCompare(tree.recent, "opened", true)
            tryCompare(tree.recent, "currentIndex", 0)
            keyClick(Qt.Key_Space)
            compare(fakeAccess.activateCalls, 1)
            compare(fakeAccess.lastId, "alpha")
            tryCompare(tree.file, "opened", false)
        }

        function test_escapeAndLeftCloseWithoutActivation() {
            const applet = createTemporaryObject(appletComponent, root)
            const tree = nativeTree(applet)
            mouseClick(tree.bar.itemAt(0))
            tryCompare(tree.file, "opened", true)
            tree.file.currentIndex = 1
            keyClick(Qt.Key_Right)
            tryCompare(tree.recent, "opened", true)
            keyClick(Qt.Key_Left)
            tryCompare(tree.recent, "opened", false)
            verify(tree.file.opened)
            keyClick(Qt.Key_Escape)
            tryCompare(tree.file, "opened", false)
            compare(fakeAccess.activateCalls, 0)
        }

        function test_providerLossDismissesAndMakesTreeInert() {
            const applet = createTemporaryObject(appletComponent, root)
            const tree = nativeTree(applet)
            mouseClick(tree.bar.itemAt(0))
            tryCompare(tree.file, "opened", true)
            fakeAccess.available = false
            tryCompare(tree.file, "opened", false)
            verify(!tree.bar.itemAt(0).enabled)
            verify(!tree.file.itemAt(0).enabled)
            tree.file.itemAt(0).triggered()
            compare(fakeAccess.activateCalls, 0)
        }

        function test_depthCapLeavesExcessSubmenuInert() {
            let leaf = {"id": "leaf", "kind": "action", "text": "Leaf",
                        "enabled": true}
            for (let depth = 8; depth >= 0; --depth) {
                leaf = {"id": "level" + depth, "kind": "submenu",
                        "text": "Level " + depth, "enabled": true,
                        "children": [leaf]}
            }
            fakeAccess.items = [leaf]
            const applet = createTemporaryObject(appletComponent, root)
            const bar = applet.menuBar
            tryCompare(bar, "count", 1)
            let menu = bar.menuAt(0)
            for (let depth = 1; depth < 6; ++depth) {
                const nested = menu.menuAt(0)
                verify(nested !== null)
                menu = nested
            }
            compare(menu.menuAt(0), null)
            verify(!menu.itemAt(0).enabled)
        }
    }
}
