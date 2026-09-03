// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.Shell.StatusNotifier 1.0 as StatusNotifierComponents
import QindaQt.Shell.StatusNotifier.Tests 1.0 as Harness

Item {
    id: testRoot
    width: 400
    height: 300

    QtObject {
        id: fakeAccess

        property string phaseText: "ready"
        property string phaseReasonText: ""
        property bool watcherLive: true
        property var itemRows: []
        property int itemCount: 0
        property int presentedCount: 0
        property int overflowCount: 0
        property string overflowText: ""
        property bool readGranted: true
        property bool activateGranted: true
        property int iconSize: 22
        property bool feedbackPresent: false
        property string feedback: ""
        property string feedbackStatus: "error"

        property int activateCalls: 0
        property int openContextMenuCalls: 0
        property int menuRowsForCalls: 0
        property int clearFeedbackCalls: 0
        property var lastActivateArgs: null
        property var lastOpenContextMenuArgs: null
        property var lastMenuRowsForArgs: null
        property var scriptedMenuRows: []

        function activateItem(uniqueName, objectPath, generation) {
            ++activateCalls
            lastActivateArgs = [uniqueName, objectPath, generation]
            return true
        }

        function secondaryActivateItem(uniqueName, objectPath, generation) {
            return true
        }

        function openContextMenu(uniqueName, objectPath, generation) {
            ++openContextMenuCalls
            lastOpenContextMenuArgs = [uniqueName, objectPath, generation]
            return true
        }

        function menuRowsFor(uniqueName, objectPath, generation) {
            ++menuRowsForCalls
            lastMenuRowsForArgs = [uniqueName, objectPath, generation]
            return scriptedMenuRows
        }

        function clearFeedback() {
            ++clearFeedbackCalls
            feedbackPresent = false
            feedback = ""
        }
    }

    function makeRow(uniqueName, generation) {
        return {
            uniqueName: uniqueName,
            objectPath: "/StatusNotifierItem",
            generation: generation,
            identity: "org.qindaqt." + uniqueName,
            title: "Item " + uniqueName,
            accessibleName: "Item " + uniqueName,
            accessibleDescription: "",
            accessibleStatusText: "active",
            needsAttention: false,
            active: true,
            hasMenu: true,
            menuEntryCount: 1,
            keyboardActivateText: "Enter or Space",
            keyboardContextMenuText: "Shift+F10 or Menu key",
            secondaryActivatePointerOnly: true,
            iconDataUrl: "",
            iconIsPlaceholder: true
        }
    }

    Component {
        id: appletComponent

        StatusNotifierComponents.StatusNotifierApplet {
            access: fakeAccess
            theme: null
        }
    }

    TestCase {
        name: "StatusNotifierAppletKeyboardTests"
        // Force the harness singleton onto this engine (publishes the
        // QST-1 theme) before any applet component is created.
        readonly property bool harnessReady: Harness.Harness.ready
        when: windowShown

        function init() {
            fakeAccess.phaseText = "ready"
            fakeAccess.itemRows = []
            fakeAccess.itemCount = 0
            fakeAccess.presentedCount = 0
            fakeAccess.overflowCount = 0
            fakeAccess.overflowText = ""
            fakeAccess.activateGranted = true
            fakeAccess.feedbackPresent = false
            fakeAccess.feedback = ""
            fakeAccess.activateCalls = 0
            fakeAccess.openContextMenuCalls = 0
            fakeAccess.menuRowsForCalls = 0
            fakeAccess.clearFeedbackCalls = 0
            fakeAccess.lastActivateArgs = null
            fakeAccess.lastOpenContextMenuArgs = null
            fakeAccess.lastMenuRowsForArgs = null
            fakeAccess.scriptedMenuRows = []
        }

        function delegatesOf(applet) {
            var repeater = findChild(applet, "statusNotifierStripRepeater")
            if (repeater === null)
                return []
            var result = []
            for (var i = 0; i < repeater.count; ++i)
                result.push(repeater.itemAt(i))
            return result
        }

        // Real Tab/Backtab traversal across the delegates, then Space and
        // Return activation with the exact generation-fenced owner key.
        function test_tabTraversalAndKeyboardActivation() {
            fakeAccess.itemRows = [makeRow(":1.42", 3), makeRow(":1.43", 4)]
            fakeAccess.itemCount = 2
            fakeAccess.presentedCount = 2

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            tryVerify(() => testRoot.visible)
            var delegates = delegatesOf(applet)
            compare(delegates.length, 2)

            delegates[0].forceActiveFocus(Qt.TabFocusReason)
            tryVerify(() => delegates[0].activeFocus)
            keyClick(Qt.Key_Tab)
            tryVerify(() => delegatesOf(applet)[1].activeFocus,
                      1000,
                      "Tab should reach the second delegate")
            keyClick(Qt.Key_Backtab)
            tryVerify(() => delegatesOf(applet)[0].activeFocus,
                      1000,
                      "Backtab should return to the first delegate")

            keyClick(Qt.Key_Return)
            tryVerify(() => fakeAccess.activateCalls === 1)
            compare(fakeAccess.lastActivateArgs, [":1.42", "/StatusNotifierItem", 3])

            keyClick(Qt.Key_Tab)
            tryVerify(() => delegatesOf(applet)[1].activeFocus)
            keyClick(Qt.Key_Space)
            tryVerify(() => fakeAccess.activateCalls === 2)
            compare(fakeAccess.lastActivateArgs, [":1.43", "/StatusNotifierItem", 4])
        }

        // Shift+F10 opens the context popup with the exact owner key; Escape
        // dismisses it without any dispatch.
        function test_shiftF10OpensContextPopupAndEscapeCloses() {
            fakeAccess.itemRows = [makeRow(":1.42", 3)]
            fakeAccess.itemCount = 1
            fakeAccess.presentedCount = 1
            fakeAccess.scriptedMenuRows = [{
                depth: 0,
                kind: "item",
                label: "Open",
                enabled: true,
                visible: true,
                hasChildren: false
            }]

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var delegate = delegatesOf(applet)[0]
            verify(delegate !== undefined && delegate !== null)

            // Capture the popup BEFORE it opens: an open popup is reparented
            // to the window overlay and leaves the applet's object subtree.
            var popup = findChild(delegate, "statusNotifierContextPopup")
            verify(popup !== null)

            delegate.forceActiveFocus(Qt.TabFocusReason)
            tryVerify(() => delegate.activeFocus)

            keyClick(Qt.Key_F10, Qt.ShiftModifier)
            tryVerify(() => fakeAccess.menuRowsForCalls === 1)
            compare(fakeAccess.lastMenuRowsForArgs, [":1.42", "/StatusNotifierItem", 3])

            tryCompare(popup, "opened", true)

            var openMenu = findChild(popup, "statusNotifierOpenMenuButton")
            verify(openMenu !== null)
            openMenu.clicked()
            compare(fakeAccess.openContextMenuCalls, 1)
            compare(fakeAccess.lastOpenContextMenuArgs, [":1.42", "/StatusNotifierItem", 3])
            tryCompare(popup, "opened", false)

            // Reopen and dismiss with Escape: no further dispatch.
            keyClick(Qt.Key_Menu)
            tryVerify(() => fakeAccess.menuRowsForCalls === 2)
            tryCompare(popup, "opened", true)
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.openContextMenuCalls, 1)
        }
    }
}
