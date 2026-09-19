// SPDX-License-Identifier: GPL-3.0-or-later
import QindaQt.Shell.GlobalMenu as GlobalMenuComponents
import QtQuick
import QtQuick.Controls
import QtTest

// Top-level popups open adjacent to the triggering menu-bar item on every panel edge, slide along
// the panel axis to stay on the output, and keep Qt-owned switching, submenus, and dismissal. Panels
// are frameless windows at exact output positions exposing RuntimePanel's `panel.edge` model.
Item {
    id: root
    width: 160
    height: 120

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
        id: panelHost
        Window {
            id: host
            property string edge: "top"
            property var panel: ({"edge": edge})
            readonly property alias applet: hostedApplet
            flags: Qt.FramelessWindowHint
            color: "transparent"
            GlobalMenuComponents.GlobalMenuApplet {
                id: hostedApplet
                access: fakeAccess
                theme: root.theme
                vertical: host.edge === "left" || host.edge === "right"
            }
        }
    }

    Component {
        id: modelessHost
        Window {
            readonly property alias applet: modelessApplet
            flags: Qt.FramelessWindowHint
            color: "transparent"
            GlobalMenuComponents.GlobalMenuApplet {
                id: modelessApplet
                access: fakeAccess
                theme: root.theme
            }
        }
    }

    TestCase {
        name: "GlobalMenuPopupPlacement"
        when: windowShown

        function entry(id, text, childCount) {
            const children = []
            for (let i = 0; i < childCount; ++i)
                children.push({"id": id + "-" + i, "kind": "action",
                               "text": text + " action " + i, "enabled": true})
            return {"id": id, "kind": "submenu", "text": text, "enabled": true,
                    "children": children}
        }

        function menuItems(fileChildren) {
            const file = entry("file", "File", fileChildren ?? 2)
            file.children.push({"id": "recent", "kind": "submenu", "text": "Recent",
                                "enabled": true, "children": [{
                                    "id": "alpha", "kind": "action", "text": "Alpha",
                                    "enabled": true}]})
            return [file, entry("edit", "Edit", 2)]
        }

        function init() {
            fakeAccess.available = true
            fakeAccess.items = menuItems()
            fakeAccess.activateCalls = 0
            fakeAccess.lastId = ""
        }

        // Panel band flush with `edge`, applet `along` its axis; a modeless host spans the output.
        function openPanel(edge, along, component) {
            const screenWidth = root.Screen.width
            const screenHeight = root.Screen.height
            const sideways = edge === "left" || edge === "right"
            const geometry = {
                "top": [0, 0, screenWidth, 32],
                "bottom": [0, screenHeight - 32, screenWidth, 32],
                "left": [0, 0, 44, screenHeight],
                "right": [screenWidth - 44, 0, 44, screenHeight]
            }[edge]
            const band = component === undefined
            const host = createTemporaryObject(component ?? panelHost, root, band
                ? {"x": geometry[0], "y": geometry[1], "width": geometry[2], "height": geometry[3]}
                : {"x": 0, "y": 0, "width": screenWidth, "height": screenHeight})
            if (band)
                host.edge = edge
            host.applet.x = (band ? 0 : geometry[0]) + (sideways ? 0 : along)
            host.applet.y = (band ? 0 : geometry[1]) + (sideways ? along : 0)
            host.applet.width = sideways ? geometry[2] : 420
            host.applet.height = sideways ? 200 : geometry[3]
            host.visible = true
            tryCompare(host.applet.menuBar, "count", 2)
            waitForRendering(host.contentItem)
            return host
        }

        function popupOffset(menu, item) {
            const popupWindow = menu.contentItem.Window.window
            const origin = item.mapToGlobal(0, 0)
            return Qt.point(popupWindow.x - origin.x, popupWindow.y - origin.y)
        }

        function openMenu(menu, item) {
            mouseClick(item)
            tryCompare(menu, "opened", true)
            compare(menu.popupType, Popup.Window)
        }

        // `expected` is read after opening (a menu lays out its rows then); poll
        // the asynchronous window move, then compare to report the actual origin.
        function comparePopupOffset(menu, item, expectedFor) {
            let expected = expectedFor()
            for (let attempt = 0; attempt < 100; ++attempt) {
                expected = expectedFor()
                const offset = popupOffset(menu, item)
                if (offset.x === expected.x && offset.y === expected.y)
                    break
                wait(20)
            }
            const offset = popupOffset(menu, item)
            compare(offset.x, expected.x, "popup x relative to the triggering item")
            compare(offset.y, expected.y, "popup y relative to the triggering item")
        }

        function test_menuOpensAdjacentToTriggeringItem_data() {
            return [{"tag": "top", "edge": "top"}, {"tag": "bottom", "edge": "bottom"},
                    {"tag": "left", "edge": "left"}, {"tag": "right", "edge": "right"}]
        }

        function test_menuOpensAdjacentToTriggeringItem(data) {
            const host = openPanel(data.edge, 90)
            const bar = host.applet.menuBar
            // The second entry proves adjacency to the exact item, not the bar.
            const item = bar.itemAt(1)
            const menu = bar.menuAt(1)
            openMenu(menu, item)
            comparePopupOffset(menu, item, function() {
                return {
                    "top": Qt.point(0, item.height), "bottom": Qt.point(0, -menu.height),
                    "left": Qt.point(item.width, 0), "right": Qt.point(-menu.width, 0)
                }[data.edge]
            })
            mouseClick(menu.itemAt(0))
            compare(fakeAccess.activateCalls, 1)
            compare(fakeAccess.lastId, "edit-0")
            tryCompare(menu, "opened", false)
        }

        function test_panelAxisSlideKeepsPopupOnOutput_data() {
            return [{"tag": "top", "edge": "top"}, {"tag": "bottom", "edge": "bottom"},
                    {"tag": "left", "edge": "left"}, {"tag": "right", "edge": "right"}]
        }

        function test_panelAxisSlideKeepsPopupOnOutput(data) {
            const sideways = data.edge === "left" || data.edge === "right"
            const host = openPanel(data.edge, (sideways ? root.Screen.height
                                                        : root.Screen.width) - 30)
            const bar = host.applet.menuBar
            const item = bar.itemAt(0)
            const menu = bar.menuAt(0)
            const origin = item.mapToGlobal(0, 0)
            openMenu(menu, item)
            comparePopupOffset(menu, item, function() {
                return sideways
                    ? Qt.point(data.edge === "left" ? item.width : -menu.width,
                               root.Screen.height - menu.height - origin.y)
                    : Qt.point(root.Screen.width - menu.width - origin.x,
                               data.edge === "top" ? item.height : -menu.height)
            })
        }

        function test_oversizedPopupCapsToOutputAndScrolls() {
            fakeAccess.items = menuItems(60)
            const host = openPanel("left", 120)
            const bar = host.applet.menuBar
            const item = bar.itemAt(0)
            const menu = bar.menuAt(0)
            openMenu(menu, item)
            verify(menu.implicitHeight > root.Screen.height, "fixture must exceed the output")
            tryCompare(menu, "height", root.Screen.height)
            tryCompare(menu.contentItem, "interactive", true)
            comparePopupOffset(menu, item, function() {
                return Qt.point(item.width, -item.mapToGlobal(0, 0).y)
            })
        }

        function test_reopenFollowsChangedAnchorGeometry() {
            const host = openPanel("bottom", 60)
            const bar = host.applet.menuBar
            const item = bar.itemAt(0)
            const menu = bar.menuAt(0)
            const above = function() { return Qt.point(0, -menu.height) }
            openMenu(menu, item)
            comparePopupOffset(menu, item, above)
            const firstX = menu.contentItem.Window.window.x
            keyClick(Qt.Key_Escape)
            tryCompare(menu, "opened", false)
            host.applet.x += 150
            waitForRendering(host.contentItem)
            openMenu(menu, item)
            comparePopupOffset(menu, item, above)
            compare(menu.contentItem.Window.window.x, firstX + 150)
        }

        function test_explicitEdgeAndModelessFallback() {
            // Model top, band on the output bottom: only the override fits the popup above.
            const host = openPanel("bottom", 40)
            host.edge = "top"
            host.applet.panelEdge = "bottom"
            const bar = host.applet.menuBar
            openMenu(bar.menuAt(0), bar.itemAt(0))
            comparePopupOffset(bar.menuAt(0), bar.itemAt(0),
                               function() { return Qt.point(0, -bar.menuAt(0).height) })
            keyClick(Qt.Key_Escape)
            tryCompare(bar.menuAt(0), "opened", false)

            const lower = openPanel("bottom", 40, modelessHost).applet.menuBar
            openMenu(lower.menuAt(0), lower.itemAt(0))
            comparePopupOffset(lower.menuAt(0), lower.itemAt(0),
                               function() { return Qt.point(0, -lower.menuAt(0).height) })
        }

        function test_placedPopupKeepsQtSwitchingSubmenuAndDismissal() {
            const host = openPanel("bottom", 60)
            const bar = host.applet.menuBar
            const file = bar.menuAt(0)
            const edit = bar.menuAt(1)
            openMenu(edit, bar.itemAt(1))
            comparePopupOffset(edit, bar.itemAt(1), function() { return Qt.point(0, -edit.height) })
            mousePress(bar.itemAt(0))
            tryCompare(file, "opened", true)
            compare(edit.opened, false)
            mouseRelease(bar.itemAt(0))
            compare(file.opened, true)
            comparePopupOffset(file, bar.itemAt(0), function() { return Qt.point(0, -file.height) })
            tryCompare(file, "activeFocus", true)
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Down)
            tryCompare(file, "currentIndex", 2)
            keyClick(Qt.Key_Right)
            const recent = file.menuAt(2)
            tryCompare(recent, "opened", true)
            tryCompare(recent, "activeFocus", true)
            keyClick(Qt.Key_Escape)
            tryCompare(recent, "opened", false)
            verify(file.opened)
            // Offscreen skips the popup grab (QTBUG-134009): send the outside press it routes.
            const outside = file.contentItem.mapFromGlobal(root.Screen.width / 2, 40)
            mouseClick(file.contentItem, outside.x, outside.y)
            tryCompare(file, "opened", false)
            compare(fakeAccess.activateCalls, 0)
        }

        // The same vectors pin ControlPopupFrame's placement contract in
        // tst_desktop_controls_qml_menus.cpp::controlPopupPlacementMath.
        function test_placementContractMatchesDesktopControls() {
            const menu = openPanel("top", 40).applet.menuBar.menuAt(0)
            function place(x, y, w, h, pw, ph, edge, bw, bh) {
                return menu.placementFor(Qt.point(x, y), w, h, pw, ph, edge, bw, bh)
            }
            compare(place(100, 0, 80, 28, 300, 200, "top", 1920, 1080), Qt.point(0, 28))
            compare(place(100, 4, 80, 28, 300, 200, "bottom", 1920, 1080), Qt.point(0, -200))
            compare(place(1800, 0, 80, 28, 300, 200, "top", 1920, 1080), Qt.point(-180, 28))
            compare(place(50, 0, 80, 28, 500, 200, "top", 300, 1080), Qt.point(-50, 28))
            compare(place(0, 1000, 48, 48, 300, 200, "left", 1920, 1080), Qt.point(48, -120))
            compare(place(1872, 10, 48, 48, 300, 200, "right", 1920, 1080), Qt.point(-300, 0))
            compare(place(1800, 0, 80, 28, 300, 200, "top", 0, 0), Qt.point(0, 28))
        }
    }
}
