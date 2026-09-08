// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import QindaQt.Shell.StatusNotifier 1.0 as Tray
import QindaQt.Shell.StatusNotifier.Tests 1.0 as Harness

Item {
    id: root
    width: 700
    height: 450
    QtObject {
        id: fakeAccess
        property bool activateGranted: true
        property bool menuOnly: false
        property bool exported: true
        property string status: "ready"
        property string revision: "9007199254740993"
        property var entries: []
        property var calls: []
        signal menuChanged()
        function itemIsMenu() { return menuOnly }
        function hasExportedMenu() { return exported }
        function menuStateFor() { return {status: status, revision: revision, entries: entries} }
        function activateItem(owner, path, generation, x, y) {
            calls.push(["activate", owner, path, generation, x, y]); return true
        }
        function openContextMenu(owner, path, generation, x, y) {
            calls.push(["menu", owner, path, generation, x, y]); return true
        }
        function secondaryActivateItem(owner, path, generation, x, y) {
            calls.push(["secondary", owner, path, generation, x, y]); return true
        }
        function scrollItem(owner, path, generation, delta, orientation) {
            calls.push(["scroll", delta, orientation]); return true
        }
        function invokeMenu(owner, path, generation, serial, id) {
            calls.push(["invoke", owner, path, generation, serial, id]); return true
        }
        function aboutToShowMenu(owner, path, generation, serial, id) {
            calls.push(["submenu", serial, id]); return true
        }
    }
    Component {
        id: delegateComponent
        Tray.StatusNotifierItemDelegate {
            x: 100; y: 100
            access: fakeAccess
            item: ({uniqueName: ":1.42", objectPath: "/StatusNotifierItem", generation: 3,
                identity: "fixture", title: "Fixture", accessibleName: "Fixture",
                accessibleStatusText: "active", accessibleDescription: "",
                keyboardActivateText: "Enter", keyboardContextMenuText: "Shift+F10",
                iconDataUrl: "", iconIsPlaceholder: true, needsAttention: false})
        }
    }
    TestCase {
        name: "StatusNotifierMenus"
        readonly property bool harnessReady: Harness.Harness.ready
        when: windowShown
        function init() {
            fakeAccess.activateGranted = true
            fakeAccess.menuOnly = false
            fakeAccess.exported = true
            fakeAccess.status = "ready"
            fakeAccess.revision = "9007199254740993"
            fakeAccess.calls = []
            fakeAccess.entries = [
                {id: 4, kind: "action", label: "Configuration…", enabled: true},
                {id: 5, kind: "action", label: "Battery-color mouse LED", enabled: true,
                 checkable: true, checked: true},
                {id: 6, kind: "action", label: "Unavailable", enabled: false},
                {id: 7, kind: "action", label: "Hidden", visible: false},
                {id: 8, kind: "submenu", label: "Profiles", enabled: true, children: [
                    {id: 9, kind: "action", label: "Default", enabled: true}]}
            ]
        }
        function test_pointerGesturesKeepCoordinatesAndWheelOrientation() {
            const delegate = createTemporaryObject(delegateComponent, root)
            verify(delegate)
            mouseClick(delegate, 15, 15, Qt.LeftButton)
            compare(fakeAccess.calls.length, 1)
            compare(fakeAccess.calls[0][0], "activate")
            const anchor = delegate.mapToGlobal(delegate.width / 2, delegate.height / 2)
            compare(fakeAccess.calls[0].slice(4), [Math.round(anchor.x), Math.round(anchor.y)])
            mouseClick(delegate, 15, 15, Qt.MiddleButton)
            compare(fakeAccess.calls[1][0], "secondary")
            mouseWheel(delegate, 15, 15, 0, 120)
            compare(fakeAccess.calls[2], ["scroll", 120, "vertical"])
            mouseWheel(delegate, 15, 15, -120, 0)
            compare(fakeAccess.calls[3], ["scroll", -120, "horizontal"])
        }
        function test_legacyContextDispatchesDirectlyWithoutIntermediatePopup() {
            fakeAccess.exported = false
            const delegate = createTemporaryObject(delegateComponent, root)
            const popup = findChild(delegate, "statusNotifierContextPopup")
            mouseClick(delegate, 15, 15, Qt.RightButton)
            compare(fakeAccess.calls.length, 1)
            compare(fakeAccess.calls[0][0], "menu")
            compare(popup.opened, false)
        }
        function test_contextMenuHasRealActionsAndKeepsRevisionPrecision() {
            const delegate = createTemporaryObject(delegateComponent, root)
            const popup = findChild(delegate, "statusNotifierContextPopup")
            mouseClick(delegate, 15, 15, Qt.RightButton)
            tryCompare(popup, "opened", true)
            tryCompare(popup, "count", 4)
            compare(popup.itemAt(1).Accessible.checkable, true)
            compare(popup.itemAt(1).Accessible.checked, true)
            compare(popup.itemAt(2).enabled, false)
            popup.currentIndex = 0
            keyClick(Qt.Key_Return)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.calls.length, 2)
            compare(fakeAccess.calls[1], ["invoke", ":1.42", "/StatusNotifierItem", 3,
                                     "9007199254740993", 4])
        }
        function test_menuOnlyPrimaryClickAndLiveUpdate() {
            fakeAccess.menuOnly = true
            const delegate = createTemporaryObject(delegateComponent, root)
            const popup = findChild(delegate, "statusNotifierContextPopup")
            mouseClick(delegate, 15, 15, Qt.LeftButton)
            tryCompare(popup, "opened", true)
            compare(fakeAccess.calls[0][0], "menu")
            fakeAccess.status = "loading"
            fakeAccess.menuChanged()
            tryCompare(popup.itemAt(0), "enabled", false)
            fakeAccess.entries = [{id: 4, kind: "action", label: "Updated settings", enabled: false}]
            fakeAccess.revision = "9007199254740994"
            fakeAccess.status = "ready"
            fakeAccess.menuChanged()
            tryCompare(popup, "count", 1)
            tryVerify(() => popup.itemAt(0).text === "Updated settings")
            compare(popup.itemAt(0).enabled, false)
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.calls.length, 1)
        }
        function test_submenuKeyboardOpeningRefreshesBeforeAction() {
            const delegate = createTemporaryObject(delegateComponent, root)
            const popup = findChild(delegate, "statusNotifierContextPopup")
            delegate.openContextPopup()
            tryCompare(popup, "opened", true)
            tryCompare(popup, "count", 4)
            popup.currentIndex = 3
            keyClick(Qt.Key_Right)
            const submenu = popup.menuAt(3)
            tryCompare(submenu, "opened", true)
            compare(fakeAccess.calls[1], ["submenu", "9007199254740993", 8])
            fakeAccess.status = "loading"
            fakeAccess.menuChanged()
            tryCompare(submenu.itemAt(0), "enabled", false)
            compare(submenu.opened, true)
            wait(20) // Deliver the coalesced status-only publication before the next tree.
            compare(fakeAccess.calls.length, 2)
            const next = JSON.parse(JSON.stringify(fakeAccess.entries))
            next[4].children[0].label = "Updated profile"
            fakeAccess.entries = next
            fakeAccess.revision = "9007199254740994"
            fakeAccess.status = "ready"
            fakeAccess.menuChanged()
            tryVerify(() => popup.menuAt(3) !== submenu && popup.menuAt(3).opened)
            const updated = popup.menuAt(3)
            compare(updated.itemAt(0).text, "Updated profile")
            compare(fakeAccess.calls.length, 2)
            updated.currentIndex = 0
            keyClick(Qt.Key_Return)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.calls[2][5], 9)
        }
    }
}
