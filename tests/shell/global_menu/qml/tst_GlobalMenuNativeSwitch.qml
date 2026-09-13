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
                compare(bar.itemAt(index).width, bar.itemAt(index).implicitWidth)
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

        // Observed on Qt 6.11.1: member reads on a destroyed QObject resolve
        // to undefined instead of throwing, so `!menu.opened` already reads
        // as total retirement for a destroyed delegate; the catch only
        // guarantees a dangling reference can never abort the row. A retired
        // menu is destroyed (removed delegate) or closed (retained delegate
        // whose projection was rebuilt); a live open popup is the defect
        // this guards against.
        function retired(menu) {
            if (menu === null)
                return true
            try {
                return !menu.opened
            } catch (error) {
                return true
            }
        }

        function test_republishWithFewerEntriesRetiresOpenPopup() {
            const bar = menuBar()
            const fileMenu = bar.menuAt(0)
            const staleAction = fileMenu.itemAt(0)
            compare(staleAction.text, "New")
            mouseClick(bar.itemAt(0))
            tryCompare(fileMenu, "opened", true)
            compare(fileMenu.popupType, Popup.Window)

            // The provider republishes a smaller top-level projection while
            // available stays true: File disappears entirely. Its open native
            // popup must be retired with the removed projection before any
            // new interaction, and the retired generation must not dispatch.
            fakeAccess.items = menuItems().slice(1)
            // Strict same-turn reads with no event-loop spin: a retirement
            // deferred through Qt.callLater would leave the bar oversized and
            // the obsolete popup open right here, so a polling check would
            // hide that defect instead of failing on it.
            compare(bar.count, 1)
            compare(bar.menuAt(0).menuData.id, "edit")
            verify(retired(fileMenu),
                   "obsolete popup must retire synchronously with the republish")
            // The stale item is not destroyed on this turn: it still reads
            // enabled and interactive. Its emitted triggered() must simply
            // not dispatch (observed behavior consistent with Qt invalidating
            // the released delegate's context; no mechanism is pinned here).
            staleAction.triggered()
            compare(fakeAccess.activateCalls, 0)

            // Only the new projection can dispatch, and it does so exactly.
            mouseClick(bar.itemAt(0))
            tryCompare(bar.menuAt(0), "opened", true)
            mouseClick(bar.menuAt(0).itemAt(0))
            compare(fakeAccess.activateCalls, 1)
            compare(fakeAccess.lastId, "undo")
        }

        function test_republishWithNoEntriesClosesEveryPopup() {
            const bar = menuBar()
            const fileMenu = bar.menuAt(0)
            mouseClick(bar.itemAt(0))
            tryCompare(fileMenu, "opened", true)

            // Empty republish with available unchanged withdraws the whole
            // top-level projection; an open popup cannot survive it. Strict
            // same-turn read: a deferred removal would still list menus here.
            fakeAccess.items = []
            compare(bar.count, 0)
            verify(retired(fileMenu),
                   "obsolete popup must retire synchronously with the empty republish")
            compare(applet.hasContent, false)
            compare(fakeAccess.activateCalls, 0)

            // Fresh truth must project into a working native bar again.
            fakeAccess.items = menuItems()
            tryCompare(bar, "count", 2)
            mouseClick(bar.itemAt(0))
            tryCompare(bar.menuAt(0), "opened", true)
            mouseClick(bar.menuAt(0).itemAt(0))
            compare(fakeAccess.activateCalls, 1)
            compare(fakeAccess.lastId, "new")
        }

        function test_downPressThenRepublishRetiresQueuedFocusTarget() {
            const bar = menuBar()
            const fileMenu = bar.menuAt(0)
            bar.itemAt(0).forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Down)
            // Offscreen, Popup.Window opens synchronously and the QTest key
            // delivery itself runs the deferred queue before returning, so
            // the press's own callback executes inside keyClick while File
            // is still open. Direct compare, no spin: polling here would
            // also drain anything queued after the press.
            compare(fileMenu.opened, true)

            // The stale ordering cannot be produced through key delivery,
            // so re-mint the exact deferred callback the press queues by
            // calling the production queueing function on the captured
            // File menu — no key event, no event-loop turn — and republish
            // before anything drains it. The republish must retire the
            // popup synchronously, and that queued callback must run only
            // after the retirement and stay inert against the fresh
            // projection.
            applet.focusFirstMenuItem(fileMenu)
            fakeAccess.items = menuItems().slice(1)
            compare(bar.count, 1)
            verify(retired(fileMenu),
                   "obsolete popup must retire synchronously with the republish")

            // Flush the queued batch inside this function: this marker
            // cannot run before the focus callback ahead of it in the
            // callLater queue has executed, so reaching it proves the stale
            // callback already ran.
            let flushed = false
            Qt.callLater(function() { flushed = true })
            tryVerify(function() { return flushed })

            // `visible` is set synchronously by open(), so a mutant whose
            // stale branch reopens the fresh projection fails right here;
            // the second-turn re-check rules out a later transitioned open.
            compare(bar.menuAt(0).visible, false,
                    "stale focus callback must leave the fresh projection closed")
            let secondTurn = false
            Qt.callLater(function() { secondTurn = true })
            tryVerify(function() { return secondTurn })
            compare(bar.menuAt(0).visible, false)
            compare(bar.menuAt(0).opened, false)
            compare(fakeAccess.activateCalls, 0)
        }

    }
}
