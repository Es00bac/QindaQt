// SPDX-License-Identifier: GPL-3.0-or-later
import QindaQt.Shell.GlobalMenu as GlobalMenuComponents
import QtQuick
import QtQuick.Controls
import QtTest

// Placement contract for the GlobalMenuPopup anchor mapping. The host
// nesting mirrors production: an offset panel strip holding an applet whose
// height differs from the 24 px entries row, so the Row is vertically
// centered and the anchor's parent frame is offset from the applet root —
// the frame confusion the old anchorItem.x/y binding shipped.
Item {
    id: root
    width: 640
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
        function activate(id) { ++activateCalls }
    }

    Item {
        id: panelStrip
        x: 137
        y: 11
        width: 480
        height: 28

        GlobalMenuComponents.GlobalMenuApplet {
            id: applet
            width: 420
            height: 40
            access: fakeAccess
            theme: root.theme
        }
    }

    TestCase {
        name: "GlobalMenuPopupPlacement"
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
            fakeAccess.items = menuItems()
            fakeAccess.activateCalls = 0
            panelStrip.x = 137
        }

        function topLevelEntries() {
            const row = findChild(applet, "globalMenuHorizontalLayout")
            verify(row !== null)
            return row.children.filter(
                child => child.objectName === "globalMenuTopLevelItem")
        }

        function popupWindowOffsetFromMain() {
            const popup = findChild(applet, "globalMenuPopup")
            const pw = popup.popupWindow
            if (pw === null)
                return null
            const mw = root.Window.window
            return Qt.point(pw.x - mw.x, pw.y - mw.y)
        }

        function test_popupWindowLandsOnAnchorBottomLeft() {
            const entries = topLevelEntries()
            compare(entries.length, 2)
            const popup = findChild(applet, "globalMenuPopup")
            popup.openMenu(fakeAccess.items[0], entries[0])
            tryCompare(popup, "opened", true)
            const expected = entries[0].mapToItem(null, 0, entries[0].height)
            tryVerify(function() {
                const actual = popupWindowOffsetFromMain()
                return actual !== null
                    && Math.abs(actual.x - expected.x) < 0.5
                    && Math.abs(actual.y - expected.y) < 0.5
            })
            popup.close()
        }

        function test_secondEntryAnchorsUnderItself() {
            const entries = topLevelEntries()
            const popup = findChild(applet, "globalMenuPopup")
            popup.openMenu(fakeAccess.items[1], entries[1])
            tryCompare(popup, "opened", true)
            const expected = entries[1].mapToItem(null, 0, entries[1].height)
            tryVerify(function() {
                const actual = popupWindowOffsetFromMain()
                return actual !== null
                    && Math.abs(actual.x - expected.x) < 0.5
                    && Math.abs(actual.y - expected.y) < 0.5
            })
            popup.close()
        }

        function test_rightEdgeAnchorClampsInsideWindow() {
            const entries = topLevelEntries()
            panelStrip.x = 560
            const entry = entries[0]
            verify(entry.mapToItem(null, 0, 0).x + 240 > root.width)
            const popup = findChild(applet, "globalMenuPopup")
            popup.openMenu(fakeAccess.items[0], entry)
            tryCompare(popup, "opened", true)
            tryVerify(function() {
                const actual = popupWindowOffsetFromMain()
                return actual !== null
                    && Math.abs(actual.x - (root.width - popup.width)) < 0.5
            })
            popup.close()
        }

        function test_nativeWaylandAnchorContract() {
            // On Wayland the compositor positions the popup server-side from
            // the popup window's "_q_waylandPopupAnchor*" properties; popup
            // x/y never reach it. Pin the exact contract QtWayland reads in
            // createPositioner(): the clicked entry's rect in panel window
            // coordinates, Menu-style dropdown edges (below, left-aligned),
            // and slide_x|slide_y|flip_y constraint adjustment.
            const entries = topLevelEntries()
            const popup = findChild(applet, "globalMenuPopup")
            popup.openMenu(fakeAccess.items[0], entries[0])
            tryCompare(popup, "opened", true)
            const win = popup.popupWindow
            verify(win !== null)
            const sceneTopLeft = entries[0].mapToItem(null, 0, 0)
            const anchorRect = win._q_waylandPopupAnchorRect
            compare(anchorRect.x, Math.round(sceneTopLeft.x))
            compare(anchorRect.y, Math.round(sceneTopLeft.y))
            compare(anchorRect.width, Math.round(entries[0].width))
            compare(anchorRect.height, Math.round(entries[0].height))
            compare(win._q_waylandPopupAnchor, Qt.BottomEdge | Qt.LeftEdge)
            compare(win._q_waylandPopupGravity, Qt.BottomEdge | Qt.RightEdge)
            compare(win._q_waylandPopupConstraintAdjustment, 11)
            popup.close()
        }

        function test_placementOriginForBoundaryDecisions() {
            const popup = findChild(applet, "globalMenuPopup")
            let p = popup.placementOriginFor(Qt.point(10, 20), 24, 240, 150,
                                             640, 720)
            compare(p.x, 10)
            compare(p.y, 44)
            // Right-edge clamp keeps the popup inside the containing width.
            p = popup.placementOriginFor(Qt.point(500, 20), 24, 240, 150,
                                         640, 720)
            compare(p.x, 400)
            compare(p.y, 44)
            // Negative origins clamp to zero.
            p = popup.placementOriginFor(Qt.point(-30, 20), 24, 240, 150,
                                         640, 720)
            compare(p.x, 0)
            compare(p.y, 44)
            // Crossing the bottom bound flips above the anchor.
            p = popup.placementOriginFor(Qt.point(10, 600), 24, 240, 150,
                                         640, 640)
            compare(p.x, 10)
            compare(p.y, 450)
            // Stays below when the flipped position would not fit either.
            p = popup.placementOriginFor(Qt.point(10, 100), 24, 240, 150,
                                         640, 250)
            compare(p.x, 10)
            compare(p.y, 124)
        }
    }
}
