// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest
import QindaQt.Shell.GlobalMenu 1.0
import QindaQt.Shell.DesktopMenu.Tests 1.0

// ADR-0260: the desktop menu through the compiled global menu renderer and
// the real facade: the bar reads like Finder's, the keyboard reaches and
// activates exactly one command, session-ending items ask first, and an
// application taking focus leaves the retained bar inert.
Item {
    id: root
    width: 900
    height: 240

    readonly property var theme: ({
        "cornerRadius": 6,
        "colors": {
            "surface": "#222624", "surfaceRaised": "#2c312e",
            "border": "#3c433f", "text": "#f2f1eb",
            "textMuted": "#a9afa9", "accent": "#8fc8b7",
            "accentText": "#10201b"
        }
    })

    TextInput {
        id: before
        objectName: "beforeDesktopMenu"
        text: "before"
        activeFocusOnTab: true
    }

    GlobalMenuApplet {
        id: applet
        anchors.left: parent.left
        anchors.top: before.bottom
        width: 860
        height: 30
        access: Harness.access
        theme: root.theme
    }

    TestCase {
        name: "DesktopMenuKeyboard"
        when: windowShown

        function init() {
            Harness.reset()
            tryCompare(applet, "available", true)
        }

        function cleanup() {
            applet.dismissMenus()
        }

        function currentText(menu) {
            const item = menu.itemAt(menu.currentIndex)
            return item !== null ? String(item.text) : ""
        }

        function openFirstMenu() {
            // A closed Popup.Window (menu or dialog) leaves the test window
            // without an active focus item on the offscreen platform while
            // `before` still reports focus (a compositor restores focus in a
            // session). Re-activate the window, re-take focus, and wait for
            // the asynchronous activation before tabbing into the bar.
            const window = root.Window.window
            window.requestActivate()
            tryVerify(function() { return window.active })
            before.focus = false
            before.forceActiveFocus(Qt.TabFocusReason)
            tryVerify(function() { return before.activeFocus })
            keyClick(Qt.Key_Tab)
            tryVerify(function() { return applet.menuBar.activeFocus })
            keyClick(Qt.Key_Down)
            const popup = applet.menuBar.menuAt(0)
            verify(popup !== null)
            compare(popup.popupType, Popup.Window)
            tryCompare(popup, "opened", true)
            tryCompare(popup, "currentIndex", 0)
            return popup
        }

        function moveTo(menu, text) {
            for (let i = 0; i < 16 && currentText(menu) !== text; ++i)
                keyClick(Qt.Key_Down)
            compare(currentText(menu), text)
        }

        function test_barPresentsTheFileManagerMenus() {
            verify(Harness.access.desktopMenuShown)
            compare(Harness.access.desktopMenuTitle, "File Manager")
            tryCompare(applet.menuBar, "count", 7)
            const titles = []
            for (let i = 0; i < applet.menuBar.count; ++i)
                titles.push(String(applet.menuBar.menuAt(i).title))
            compare(titles, ["File Manager", "File", "Edit", "View", "Go", "Window", "Help"])
            compare(String(applet.Accessible.name), "Application menu")
        }

        function test_keyboardReachesAndActivatesExactlyOneCommand() {
            const popup = openFirstMenu()
            compare(currentText(popup), "About This Computer")
            moveTo(popup, "System Settings…")
            keyClick(Qt.Key_Space)
            tryCompare(popup, "opened", false)
            compare(Harness.performed, ["SystemSettings"])
        }

        function test_escapeClosesWithoutActivation() {
            const popup = openFirstMenu()
            moveTo(popup, "Lock Screen")
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
            compare(Harness.performed, [])
        }

        function test_sessionEndingItemsAskFirst() {
            let popup = openFirstMenu()
            moveTo(popup, "Shut Down…")
            keyClick(Qt.Key_Space)
            tryCompare(popup, "opened", false)
            const dialog = findChild(applet, "globalMenuConfirmation")
            verify(dialog !== null)
            tryCompare(dialog, "opened", true)
            compare(String(dialog.title), "Shut down?")
            compare(Harness.performed, [])
            // Declining (Cancel, Escape, a press outside) runs nothing.
            dialog.reject()
            tryCompare(dialog, "opened", false)
            compare(Harness.performed, [])
            compare(String(Harness.access.confirmation.token ?? ""), "")

            popup = openFirstMenu()
            moveTo(popup, "Shut Down…")
            keyClick(Qt.Key_Space)
            tryCompare(dialog, "opened", true)
            dialog.accept()
            tryCompare(dialog, "opened", false)
            compare(Harness.performed, ["ShutDown"])
        }

        function test_applicationFocusLeavesTheBarInert() {
            Harness.applicationBecameActive()
            tryCompare(applet, "available", false)
            // Still painted, so the panel does not reflow while the
            // application's own menu is on its way.
            compare(applet.menuBar.count, 7)
            verify(!applet.menuBar.menuAt(0).enabled)
            compare(Harness.performed, [])
        }
    }
}
