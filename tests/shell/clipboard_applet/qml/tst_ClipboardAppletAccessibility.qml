// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.Shell.ClipboardApplet 1.0 as ClipboardComponents
import QindaQt.Shell.ClipboardApplet.Tests 1.0 as Harness

Item {
    id: testRoot
    width: 400
    height: 600

    QtObject {
        id: fakeController

        property string phaseText: "ready"
        property string phaseReasonText: ""
        property bool isLocked: false
        property bool clipboardWriteGranted: true
        property bool isHistoryEnabled: true
        property var entryRows: []
        property int entryCount: 0
        property int pinnedCount: 0
        property int unpinnedCount: 0
        property var totalPayloadBytes: 0
        property string totalPayloadBytesFormatted: "0 B"
        property bool isSearchActive: false
        property string searchQuery: ""
        property int searchResultCount: 0
        property bool searchTruncated: false
        property string emptyReasonText: "Clipboard history is empty."
        property bool feedbackPresent: false
        property string feedback: ""

        property int clearFeedbackCalls: 0

        function clearFeedback() {
            ++clearFeedbackCalls
            feedbackPresent = false
            feedback = ""
        }
    }

    Component {
        id: appletComponent

        ClipboardComponents.ClipboardApplet {
            width: 380
            controller: fakeController
        }
    }

    Component {
        id: rowComponent

        ClipboardComponents.ClipboardEntryRow {
            width: 380
            controller: fakeController
            entry: ({
                generation: 1,
                serial: 1,
                idString: "1:1",
                preview: "Accessible test content",
                previewTruncated: false,
                sourceLabel: "Terminal",
                pinned: true,
                formatsSummary: "text/plain (23 B)",
                primaryMediaType: "text/plain",
                isText: true,
                isImage: false,
                isUriList: false,
                totalBytes: 23,
                admittedTick: 100,
                lastUsedTick: 100,
                accessibleName: "Entry 1: text/plain, pinned, from Terminal, preview: 'Accessible test content'",
                accessibleDescription: "text/plain (23 B); size: 23 B; id: 1:1",
                pending: false
            })
        }
    }

    TestCase {
        name: "ClipboardAppletAccessibilityTests"
        // Force the harness singleton onto this engine (publishes the
        // QST-1 theme) before any applet component is created.
        readonly property bool harnessReady: Harness.Harness.ready
        when: windowShown

        function test_appletGroupingRoleAndDescription() {
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            compare(applet.Accessible.role, Accessible.Grouping)
            compare(applet.Accessible.name, "Clipboard")

            // Test description when locked
            fakeController.phaseText = "locked"
            verify(applet.Accessible.description.length > 0)
        }

        function test_entryRowAccessibility() {
            var row = createTemporaryObject(rowComponent, testRoot)
            verify(row !== null)

            compare(row.Accessible.role, Accessible.ListItem)
            verify(row.Accessible.name.indexOf("Entry 1") !== -1)
            verify(row.Accessible.name.indexOf("pinned") !== -1)
            verify(row.Accessible.description.indexOf("23 B") !== -1)
        }

        function test_searchFieldAccessibility() {
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var searchField = findChild(applet, "clipboardSearchField")
            verify(searchField !== null)
            compare(searchField.Accessible.role, Accessible.EditableText)
            compare(searchField.Accessible.name, "Search clipboard history")
        }

        // P2 coverage: every interactive element carries an explicit role,
        // name, description, and honest enabled/busy state.
        function makeEntry(serial, extra) {
            var entry = {
                generation: 1,
                serial: serial,
                pinned: false,
                preview: "entry " + serial,
                formatsSummary: "text/plain (7 B)",
                accessibleName: "Entry 1",
                accessibleDescription: "",
                isText: true,
                isImage: false,
                isUriList: false,
                sourceLabel: "Editor",
                pending: false
            }
            for (var key in extra)
                entry[key] = extra[key]
            return entry
        }

        function init() {
            fakeController.phaseText = "ready"
            fakeController.clipboardWriteGranted = true
            fakeController.entryRows = []
            fakeController.entryCount = 0
            fakeController.isSearchActive = false
            fakeController.searchQuery = ""
            fakeController.feedbackPresent = false
            fakeController.feedback = ""
            fakeController.clearFeedbackCalls = 0
        }

        function test_rowActionButtonsAccessibility() {
            fakeController.entryRows = [makeEntry(21)]
            fakeController.entryCount = 1
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)

            var pin = findChild(applet, "pinButton")
            verify(pin !== null)
            compare(pin.Accessible.role, Accessible.Button)
            compare(pin.Accessible.name, "Pin")
            compare(pin.Accessible.description, "Pin entry")
            compare(pin.enabled, true)
            compare(pin.busy, false)

            var del = findChild(applet, "deleteButton")
            verify(del !== null)
            compare(del.Accessible.role, Accessible.Button)
            compare(del.Accessible.name, "Delete")
            compare(del.Accessible.description, "Delete this clipboard entry")
            compare(del.enabled, true)
        }

        function test_rowActionButtonStateAccessibility() {
            // Pinned entry: the pin button announces the unpin action.
            fakeController.entryRows = [makeEntry(22, { pinned: true })]
            fakeController.entryCount = 1
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)
            var pin = findChild(applet, "pinButton")
            compare(pin.Accessible.name, "Unpin")
            compare(pin.Accessible.description, "Unpin entry")

            // Pending entry: busy state is announced and the control is
            // disabled against double dispatch. Delegates are recreated on a
            // model change, so children must be re-found, never cached.
            fakeController.entryRows = [makeEntry(23, { pending: true })]
            tryVerify(() => {
                var p = findChild(applet, "pinButton")
                return p !== null && p.busy === true
            })
            pin = findChild(applet, "pinButton")
            compare(pin.Accessible.name, "Pin, busy")
            compare(pin.enabled, false)
            var del = findChild(applet, "deleteButton")
            compare(del.busy, true)
            compare(del.Accessible.name, "Delete, busy")
            compare(del.enabled, false)

            // Write denial: mutating controls are disabled for AT too.
            fakeController.entryRows = [makeEntry(24)]
            fakeController.clipboardWriteGranted = false
            tryVerify(() => {
                var p2 = findChild(applet, "pinButton")
                return p2 !== null && p2.busy === false
            })
            pin = findChild(applet, "pinButton")
            compare(pin.enabled, false)
            del = findChild(applet, "deleteButton")
            compare(del.enabled, false)
        }

        function test_clearButtonsAccessibility() {
            fakeController.entryRows = [makeEntry(25)]
            fakeController.entryCount = 1
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var clearUnpinned = findChild(applet, "clearUnpinnedButton")
            verify(clearUnpinned !== null)
            compare(clearUnpinned.Accessible.role, Accessible.Button)
            compare(clearUnpinned.Accessible.name, "Clear Unpinned")
            compare(clearUnpinned.Accessible.description, "Clear all unpinned clipboard items")
            compare(clearUnpinned.enabled, true)

            var clearAll = findChild(applet, "clearAllButton")
            verify(clearAll !== null)
            compare(clearAll.Accessible.role, Accessible.Button)
            compare(clearAll.Accessible.name, "Clear All")
            compare(clearAll.Accessible.description, "Clear all clipboard items including pinned")
            compare(clearAll.enabled, true)

            // Write denial disables both for assistive technology as well.
            fakeController.clipboardWriteGranted = false
            compare(clearUnpinned.enabled, false)
            compare(clearAll.enabled, false)
        }

        function test_searchClearButtonAccessibility() {
            fakeController.isSearchActive = true
            fakeController.searchQuery = "query"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var clearSearch = findChild(applet, "clearSearchButton")
            verify(clearSearch !== null)
            compare(clearSearch.visible, true)
            compare(clearSearch.Accessible.role, Accessible.Button)
            compare(clearSearch.Accessible.name, "Clear")
            compare(clearSearch.Accessible.description, "Clear search query")
            compare(clearSearch.enabled, true)
        }

        function test_feedbackDismissalAccessibility() {
            fakeController.feedbackPresent = true
            fakeController.feedback = "Could not delete the entry."
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            // Error feedback is an alert: assertive announcement surface.
            var card = findChild(applet, "clipboardFeedbackState")
            verify(card !== null)
            compare(card.visible, true)
            compare(card.Accessible.role, Accessible.AlertMessage)
            compare(card.Accessible.name, "Clipboard Notice")
            compare(card.Accessible.description, "Could not delete the entry.")

            var dismiss = findChild(card, "stateCardAction")
            verify(dismiss !== null)
            compare(dismiss.visible, true)
            compare(dismiss.Accessible.role, Accessible.Button)
            compare(dismiss.Accessible.name, "Dismiss")
            compare(dismiss.enabled, true)
        }
    }
}
