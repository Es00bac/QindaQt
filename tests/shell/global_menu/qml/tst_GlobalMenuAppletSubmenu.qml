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

    TextInput {
        id: focusSink
        objectName: "globalMenuFocusSink"
        anchors.bottom: parent.bottom
        text: "outside"
    }

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
        name: "GlobalMenuSubmenu"
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
            fakeAccess.items = menuItems()
            fakeAccess.activateCalls = 0
            fakeAccess.lastId = ""
        }

        function test_keyboardTraversalActivatesExactlyOnce() {
            const applet = createTemporaryObject(appletComponent, root)
            verify(applet !== null)
            const top = findChild(applet, "globalMenuTopLevelItem")
            verify(top !== null)
            top.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Down)
            const popup = findChild(applet, "globalMenuPopup")
            const list = findChild(applet, "globalMenuPopupList")
            tryCompare(popup, "opened", true)
            tryCompare(list, "currentIndex", 0)
            keyClick(Qt.Key_Down)
            compare(list.currentIndex, 1)
            keyClick(Qt.Key_Right)
            compare(popup.menuStack.length, 2)
            tryCompare(list, "currentIndex", 0)
            keyClick(Qt.Key_Space)
            compare(fakeAccess.activateCalls, 1)
            compare(fakeAccess.lastId, "alpha")
            tryCompare(popup, "opened", false)
        }

        function test_escapeAndLeftCloseWithoutActivation() {
            const applet = createTemporaryObject(appletComponent, root)
            const top = findChild(applet, "globalMenuTopLevelItem")
            top.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Down)
            const popup = findChild(applet, "globalMenuPopup")
            const list = findChild(applet, "globalMenuPopupList")
            tryCompare(popup, "opened", true)
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Right)
            compare(popup.menuStack.length, 2)
            keyClick(Qt.Key_Left)
            compare(popup.menuStack.length, 1)
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.activateCalls, 0)
        }

        function test_focusLossClosesWithoutActivation() {
            const applet = createTemporaryObject(appletComponent, root)
            const top = findChild(applet, "globalMenuTopLevelItem")
            top.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Down)
            const popup = findChild(applet, "globalMenuPopup")
            tryCompare(popup, "opened", true)
            focusSink.Window.window.requestActivate()
            tryCompare(focusSink.Window.window, "active", true)
            focusSink.forceActiveFocus(Qt.OtherFocusReason)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.activateCalls, 0)
        }

        function test_depthCapAndAccessibleState() {
            const applet = createTemporaryObject(appletComponent, root)
            const popup = findChild(applet, "globalMenuPopup")
            let leaf = {"id": "leaf", "kind": "action", "text": "Leaf",
                        "enabled": true}
            for (let depth = 8; depth >= 0; --depth) {
                leaf = {"id": "level" + depth, "kind": "submenu",
                        "text": "Level " + depth, "enabled": true,
                        "children": [leaf]}
            }
            popup.openMenu(leaf, applet)
            tryCompare(popup, "opened", true)
            for (let step = 0; step < 9; ++step)
                popup.choose(popup.currentItems[0])
            compare(popup.menuStack.length, popup.maximumDepth)
            wait(20)
            const list = findChild(applet, "globalMenuPopupList")
            verify(list !== null)
            const item = list.itemAtIndex(0)
            verify(item !== null)
            const button = findChild(item, "globalMenuPopupButton")
            verify(button !== null)
            compare(button.Accessible.role, Accessible.MenuItem)
            verify(button.Accessible.name.length > 0)
            popup.close()
        }
    }
}
