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

        property int clearFeedbackCalls: 0

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
            return true
        }

        function secondaryActivateItem(uniqueName, objectPath, generation) {
            return true
        }

        function openContextMenu(uniqueName, objectPath, generation) {
            return true
        }

        function menuRowsFor(uniqueName, objectPath, generation) {
            return []
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
            hasMenu: true,
            menuEntryCount: 2,
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
        name: "StatusNotifierAppletAccessibilityTests"
        // Force the harness singleton onto this engine (publishes the
        // QST-1 theme) before any applet component is created.
        readonly property bool harnessReady: Harness.Harness.ready
        when: windowShown

        function init() {
            fakeAccess.phaseText = "ready"
            fakeAccess.phaseReasonText = ""
            fakeAccess.itemRows = []
            fakeAccess.itemCount = 0
            fakeAccess.presentedCount = 0
            fakeAccess.overflowCount = 0
            fakeAccess.overflowText = ""
            fakeAccess.readGranted = true
            fakeAccess.activateGranted = true
            fakeAccess.feedbackPresent = false
            fakeAccess.feedback = ""
            fakeAccess.clearFeedbackCalls = 0
        }

        function test_appletGroupingRoleAndPhaseDescriptions() {
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            compare(applet.Accessible.role, Accessible.Grouping)
            compare(applet.Accessible.name, "Status tray")

            fakeAccess.phaseText = "loading"
            compare(applet.Accessible.description, "Status items are loading")
            fakeAccess.phaseText = "empty"
            compare(applet.Accessible.description, "No status items")
            fakeAccess.phaseText = "degraded"
            fakeAccess.phaseReasonText = "status-notifier-watcher-unavailable"
            verify(applet.Accessible.description.indexOf("limited") !== -1)
            verify(applet.Accessible.description.indexOf(
                       "status-notifier-watcher-unavailable") !== -1)
            fakeAccess.phaseText = "unavailable"
            fakeAccess.phaseReasonText = "status-items-read-not-granted"
            verify(applet.Accessible.description.indexOf("unavailable") !== -1)
        }

        function test_delegateAccessibility() {
            fakeAccess.itemRows = [makeRow(":1.42", {
                needsAttention: true,
                accessibleStatusText: "needs attention"
            })]
            fakeAccess.itemCount = 1
            fakeAccess.presentedCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var delegate = findChild(applet, "statusNotifierItemDelegate")
            verify(delegate !== null)
            compare(delegate.Accessible.role, Accessible.Button)
            compare(delegate.Accessible.name, "Item :1.42")
            verify(delegate.Accessible.description.indexOf("needs attention") !== -1)
            verify(delegate.Accessible.description.indexOf("Description for") !== -1)
            verify(delegate.Accessible.description.indexOf("Enter or Space") !== -1)
            verify(delegate.Accessible.description.indexOf("Shift+F10") !== -1)
            compare(delegate.enabled, true)

            // The icon visuals are presentation-only; the delegate carries
            // the accessible identity.
            var icon = findChild(delegate, "statusNotifierItemIcon")
            verify(icon !== null)
            compare(icon.Accessible.ignored, true)
        }

        function test_overflowChipAccessibility() {
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
            compare(chip.Accessible.role, Accessible.StaticText)
            compare(chip.Accessible.name, "3 more items")
        }

        function test_feedbackAlertAccessibility() {
            fakeAccess.feedbackPresent = true
            fakeAccess.feedback = "This status item is no longer available."
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            // Error feedback is an alert: assertive announcement surface.
            var popup = findChild(applet, "statusNotifierFeedbackPopup")
            verify(popup !== null)
            tryCompare(popup, "opened", true)
            var card = findChild(applet, "statusNotifierFeedbackCard")
            verify(card !== null)
            compare(card.Accessible.role, Accessible.AlertMessage)
            compare(card.Accessible.name, "Status Tray Notice")
            compare(card.Accessible.description,
                    "This status item is no longer available.")

            var dismiss = findChild(applet, "statusNotifierFeedbackDismiss")
            verify(dismiss !== null)
            compare(dismiss.visible, true)
            compare(dismiss.Accessible.role, Accessible.Button)
            compare(dismiss.Accessible.name, "Dismiss")
            compare(dismiss.enabled, true)
            dismiss.clicked()
            compare(fakeAccess.clearFeedbackCalls, 1)
        }

        function test_stateSurfacesAccessibility() {
            fakeAccess.phaseText = "unavailable"
            fakeAccess.phaseReasonText = "status-items-read-not-granted"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var notice = findChild(applet, "statusNotifierUnavailableNotice")
            verify(notice !== null)
            compare(notice.Accessible.role, Accessible.AlertMessage)
            compare(notice.Accessible.name, "Status tray is unavailable")
            compare(notice.Accessible.description, "status-items-read-not-granted")
        }

        function test_notConnectedSurfaceAccessibility() {
            var applet = createTemporaryObject(appletComponent, testRoot, {
                access: null
            })
            verify(applet !== null)
            var placeholder = findChild(applet, "statusNotifierNotConnectedState")
            verify(placeholder !== null)
            compare(placeholder.visible, false)
            compare(applet.Accessible.description, "Status tray controls are not connected")
        }
    }
}
