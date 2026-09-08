// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
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
        property int secondaryActivateCalls: 0
        property int openContextMenuCalls: 0
        property int menuRowsForCalls: 0
        property int clearFeedbackCalls: 0
        property var scriptedMenuRows: []

        signal menuChanged()
        function itemIsMenu(owner, path, generation) { return false }
        function hasExportedMenu(owner, path, generation) { return true }
        function menuStateFor(owner, path, generation) {
            menuRowsFor(owner, path, generation)
            return {status: "ready", revision: "1", entries: [
                {id: 4, kind: "action", label: "Configuration…", enabled: true}
            ]}
        }
        function invokeMenu(owner, path, generation, revision, itemId) { return true }
        function aboutToShowMenu(owner, path, generation, revision, itemId) { return true }
        function scrollItem(owner, path, generation, delta, orientation) { return true }

        function activateItem(uniqueName, objectPath, generation) {
            ++activateCalls
            return true
        }

        function secondaryActivateItem(uniqueName, objectPath, generation) {
            ++secondaryActivateCalls
            return true
        }

        function openContextMenu(uniqueName, objectPath, generation) {
            ++openContextMenuCalls
            return true
        }

        function menuRowsFor(uniqueName, objectPath, generation) {
            ++menuRowsForCalls
            return scriptedMenuRows
        }

        function clearFeedback() {
            ++clearFeedbackCalls
            feedbackPresent = false
            feedback = ""
        }
    }

    function makeRow(uniqueName, extra) {
        var row = {
            uniqueName: uniqueName,
            objectPath: "/StatusNotifierItem",
            generation: 3,
            identity: "org.qindaqt." + uniqueName,
            title: "Item " + uniqueName,
            accessibleName: "Item " + uniqueName,
            accessibleDescription: "Description for " + uniqueName,
            accessibleStatusText: "active",
            needsAttention: false,
            active: true,
            hasMenu: false,
            menuEntryCount: 0,
            keyboardActivateText: "Enter or Space",
            keyboardContextMenuText: "Shift+F10 or Menu key",
            secondaryActivatePointerOnly: true,
            iconDataUrl: "",
            iconIsPlaceholder: true
        }
        for (var key in extra)
            row[key] = extra[key]
        return row
    }

    Component {
        id: appletComponent

        StatusNotifierComponents.StatusNotifierApplet {
            access: fakeAccess
            theme: null
        }
    }

    TestCase {
        name: "StatusNotifierAppletTests"
        // Force the harness singleton onto this engine (publishes the
        // QST-1 theme) before any applet component is created.
        readonly property bool harnessReady: Harness.Harness.ready
        when: windowShown

        function init() {
            fakeAccess.phaseText = "ready"
            fakeAccess.phaseReasonText = ""
            fakeAccess.watcherLive = true
            fakeAccess.itemRows = []
            fakeAccess.itemCount = 0
            fakeAccess.presentedCount = 0
            fakeAccess.overflowCount = 0
            fakeAccess.overflowText = ""
            fakeAccess.readGranted = true
            fakeAccess.activateGranted = true
            fakeAccess.iconSize = 22
            fakeAccess.feedbackPresent = false
            fakeAccess.feedback = ""
            fakeAccess.feedbackStatus = "error"
            fakeAccess.activateCalls = 0
            fakeAccess.secondaryActivateCalls = 0
            fakeAccess.openContextMenuCalls = 0
            fakeAccess.menuRowsForCalls = 0
            fakeAccess.clearFeedbackCalls = 0
            fakeAccess.scriptedMenuRows = []
        }

        function test_loadingState() {
            fakeAccess.phaseText = "loading"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var loading = findChild(applet, "statusNotifierLoadingState")
            verify(loading !== null)
            compare(loading.visible, false)

            var strip = findChild(applet, "statusNotifierStripLoader")
            compare(strip.visible, false)
        }

        function test_emptyState() {
            fakeAccess.phaseText = "empty"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var empty = findChild(applet, "statusNotifierEmptyState")
            verify(empty !== null)
            compare(empty.visible, false)
        }

        function test_unavailableState() {
            fakeAccess.phaseText = "unavailable"
            fakeAccess.phaseReasonText = "status-items-read-not-granted"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var unavailable = findChild(applet, "statusNotifierUnavailableNotice")
            verify(unavailable !== null)
            compare(unavailable.visible, false)
            compare(unavailable.reason, "status-items-read-not-granted")

            var strip = findChild(applet, "statusNotifierStripLoader")
            compare(strip.visible, false)
        }

        function test_degradedStateKeepsItems() {
            fakeAccess.phaseText = "degraded"
            fakeAccess.phaseReasonText = "status-notifier-watcher-unavailable"
            fakeAccess.watcherLive = false
            fakeAccess.itemRows = [makeRow(":1.42")]
            fakeAccess.itemCount = 1
            fakeAccess.presentedCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var degraded = findChild(applet, "statusNotifierDegradedNotice")
            verify(degraded !== null)
            compare(degraded.visible, false)
            compare(degraded.reason, "status-notifier-watcher-unavailable")

            // Last-known-good rows stay visible.
            var strip = findChild(applet, "statusNotifierStripLoader")
            compare(strip.visible, true)
            var delegate = findChild(applet, "statusNotifierItemDelegate")
            verify(delegate !== null)
            tryVerify(() => delegate.visible)
        }

        function test_readyRowsRender() {
            fakeAccess.itemRows = [makeRow(":1.42"), makeRow(":1.43", {
                needsAttention: true,
                accessibleStatusText: "needs attention"
            })]
            fakeAccess.itemCount = 2
            fakeAccess.presentedCount = 2

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var strip = findChild(applet, "statusNotifierStripLoader")
            verify(strip !== null)
            compare(strip.visible, true)

            var repeater = findChild(applet, "statusNotifierStripRepeater")
            verify(repeater !== null)
            compare(repeater.count, 2)

            var first = repeater.itemAt(0)
            var second = repeater.itemAt(1)
            verify(first !== null && second !== null)

            // Placeholder truth: an empty data URL renders the badge.
            var badge = findChild(first, "statusNotifierIconPlaceholder")
            verify(badge !== null)
            compare(badge.visible, true)

            var calmBadge = findChild(first, "statusNotifierAttentionBadge")
            compare(calmBadge.visible, false)
            var attentionBadge = findChild(second, "statusNotifierAttentionBadge")
            compare(attentionBadge.visible, true)
        }

        function test_overflowChipAnnouncesTruth() {
            fakeAccess.itemRows = [makeRow(":1.42")]
            fakeAccess.itemCount = 27
            fakeAccess.presentedCount = 24
            fakeAccess.overflowCount = 3
            fakeAccess.overflowText = "3 more items"

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var chip = findChild(applet, "statusNotifierOverflowChip")
            verify(chip !== null)
            compare(chip.visible, true)
            compare(chip.Accessible.name, "3 more items")

            fakeAccess.overflowCount = 0
            fakeAccess.overflowText = ""
            compare(chip.visible, false)
        }

        function test_nullAccessShowsTruthfulPlaceholder() {
            var applet = createTemporaryObject(appletComponent, testRoot, {
                access: null
            })
            verify(applet !== null)

            var placeholder = findChild(applet, "statusNotifierNotConnectedState")
            verify(placeholder !== null)
            compare(placeholder.visible, false)

            var strip = findChild(applet, "statusNotifierStripLoader")
            compare(strip.visible, false)
        }

        function test_feedbackSurfaceDismisses() {
            fakeAccess.feedbackPresent = true
            fakeAccess.feedback = "The status item refused the request (stale)."
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var card = findChild(applet, "statusNotifierFeedbackCard")
            verify(card !== null)
            compare(card.visible, true)
            compare(card.message, "The status item refused the request (stale).")

            var popup = findChild(applet, "statusNotifierFeedbackPopup")
            verify(popup !== null)
            compare(popup.popupType, Popup.Window)
            tryCompare(popup, "opened", true)
            var dismiss = findChild(popup, "statusNotifierFeedbackDismiss")
            verify(dismiss !== null)
            tryVerify(function() { return dismiss.activeFocus })
            keyClick(Qt.Key_Space)
            compare(fakeAccess.clearFeedbackCalls, 1)
        }

        function test_feedbackWindowClosesOnEscape() {
            fakeAccess.feedbackPresent = true
            fakeAccess.feedback = "A bounded notice"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var popup = findChild(applet, "statusNotifierFeedbackPopup")
            verify(popup !== null)
            compare(popup.popupType, Popup.Window)
            tryCompare(popup, "opened", true)
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
            compare(fakeAccess.clearFeedbackCalls, 0)
        }
    }
}
