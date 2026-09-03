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

        property int selectCalls: 0
        property int deleteCalls: 0
        property int togglePinCalls: 0
        property int clearCalls: 0
        property bool lastClearUnpinnedOnly: false
        property int clearSearchCalls: 0
        property int clearFeedbackCalls: 0
        property var lastTogglePinArgs: null
        property var lastDeleteArgs: null

        function selectEntry(gen, ser) {
            ++selectCalls
            return true
        }

        function deleteEntry(gen, ser) {
            ++deleteCalls
            lastDeleteArgs = [gen, ser]
            return true
        }

        function togglePin(gen, ser) {
            ++togglePinCalls
            lastTogglePinArgs = [gen, ser]
            return true
        }

        function clearHistory(unpinnedOnly) {
            ++clearCalls
            lastClearUnpinnedOnly = unpinnedOnly
            return true
        }

        function setSearchQuery(query) {
            searchQuery = query
        }

        function clearSearch() {
            ++clearSearchCalls
            searchQuery = ""
            isSearchActive = false
        }

        function clearFeedback() {
            ++clearFeedbackCalls
            feedbackPresent = false
            feedback = ""
        }
    }

    function makeEntry(serial, extra) {
        var entry = {
            generation: 1,
            serial: serial,
            pinned: false,
            preview: "entry " + serial,
            formatsSummary: "text/plain (7 B)",
            accessibleName: "Entry " + serial,
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
                serial: 5,
                idString: "1:5",
                preview: "Keyboard navigation test",
                previewTruncated: false,
                sourceLabel: "Editor",
                pinned: false,
                formatsSummary: "text/plain (24 B)",
                primaryMediaType: "text/plain",
                isText: true,
                isImage: false,
                isUriList: false,
                totalBytes: 24,
                admittedTick: 100,
                lastUsedTick: 100,
                accessibleName: "Entry 1: text/plain",
                accessibleDescription: "",
                pending: false
            })
        }
    }

    TestCase {
        name: "ClipboardAppletKeyboardTests"
        // Force the harness singleton onto this engine (publishes the
        // QST-1 theme) before any applet component is created.
        readonly property bool harnessReady: Harness.Harness.ready
        when: windowShown

        function init() {
            fakeController.phaseText = "ready"
            fakeController.clipboardWriteGranted = true
            fakeController.entryRows = []
            fakeController.entryCount = 0
            fakeController.isSearchActive = false
            fakeController.searchQuery = ""
            fakeController.feedbackPresent = false
            fakeController.feedback = ""
            fakeController.selectCalls = 0
            fakeController.deleteCalls = 0
            fakeController.togglePinCalls = 0
            fakeController.clearCalls = 0
            fakeController.clearSearchCalls = 0
            fakeController.clearFeedbackCalls = 0
            fakeController.lastTogglePinArgs = null
            fakeController.lastDeleteArgs = null
        }

        function test_rowKeyboardReturnKey() {
            var row = createTemporaryObject(rowComponent, testRoot)
            verify(row !== null)
            row.forceActiveFocus()
            verify(row.activeFocus)

            keyClick(Qt.Key_Return)
            compare(fakeController.selectCalls, 1)
        }

        function test_rowKeyboardDeleteKey() {
            var row = createTemporaryObject(rowComponent, testRoot)
            verify(row !== null)
            row.forceActiveFocus()
            verify(row.activeFocus)

            keyClick(Qt.Key_Delete)
            compare(fakeController.deleteCalls, 1)
        }

        // P2 coverage: real Tab/Backtab traversal across the whole applet
        // surface, then keyboard (Space) activation of every interactive
        // element — search clear, both history clears, feedback dismissal,
        // Pin, Delete.
        function test_tabTraversalCoversEveryInteractiveElement() {
            fakeController.entryRows = [makeEntry(5)]
            fakeController.entryCount = 1
            fakeController.searchQuery = "abc" // makes the search-clear button visible
            fakeController.feedbackPresent = true
            fakeController.feedback = "Could not delete the entry."

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)

            var searchField = findChild(applet, "clipboardSearchField")
            var clearSearch = findChild(applet, "clearSearchButton")
            var clearUnpinned = findChild(applet, "clearUnpinnedButton")
            var clearAll = findChild(applet, "clearAllButton")
            var card = findChild(applet, "clipboardFeedbackState")
            var dismiss = findChild(card, "stateCardAction")
            var row = findChild(applet, "clipboardEntryRow")
            var pin = findChild(applet, "pinButton")
            var del = findChild(applet, "deleteButton")
            verify(searchField && clearSearch && clearUnpinned && clearAll
                   && dismiss && row && pin && del)
            verify(clearSearch.visible && dismiss.visible)

            var chain = [searchField, clearSearch, clearUnpinned, clearAll,
                         dismiss, row, pin, del]

            searchField.forceActiveFocus()
            tryVerify(() => searchField.activeFocus)

            // Tab forward visits every interactive element exactly in layout
            // order; Backtab retraces the chain in reverse.
            for (var i = 1; i < chain.length; ++i) {
                keyClick(Qt.Key_Tab)
                tryVerify(() => chain[i].activeFocus,
                          1000,
                          "Tab " + i + " expected " + chain[i].objectName)
            }
            for (var j = chain.length - 2; j >= 0; --j) {
                keyClick(Qt.Key_Backtab)
                tryVerify(() => chain[j].activeFocus,
                          1000,
                          "Backtab " + j + " expected " + chain[j].objectName)
            }
        }

        function test_keyboardActivationOfEveryControl() {
            fakeController.entryRows = [makeEntry(5)]
            fakeController.entryCount = 1
            fakeController.searchQuery = "abc"
            fakeController.feedbackPresent = true
            fakeController.feedback = "Could not delete the entry."

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)

            var activate = function(item) {
                item.forceActiveFocus()
                tryVerify(() => item.activeFocus)
                keyClick(Qt.Key_Space)
            }

            // Pin / Delete on the row forward the exact entry identity.
            var pin = findChild(applet, "pinButton")
            activate(pin)
            tryVerify(() => fakeController.togglePinCalls === 1)
            compare(fakeController.lastTogglePinArgs, [1, 5])
            compare(fakeController.selectCalls, 0)

            var del = findChild(applet, "deleteButton")
            activate(del)
            tryVerify(() => fakeController.deleteCalls === 1)
            compare(fakeController.lastDeleteArgs, [1, 5])

            // Both history-clear buttons with their exact scope argument.
            var clearUnpinned = findChild(applet, "clearUnpinnedButton")
            activate(clearUnpinned)
            tryVerify(() => fakeController.clearCalls === 1)
            compare(fakeController.lastClearUnpinnedOnly, true)

            var clearAll = findChild(applet, "clearAllButton")
            activate(clearAll)
            tryVerify(() => fakeController.clearCalls === 2)
            compare(fakeController.lastClearUnpinnedOnly, false)

            // Search clear dismisses the live query.
            var clearSearch = findChild(applet, "clearSearchButton")
            activate(clearSearch)
            tryVerify(() => fakeController.clearSearchCalls === 1)

            // Feedback dismissal reaches the controller.
            var card = findChild(applet, "clipboardFeedbackState")
            var dismiss = findChild(card, "stateCardAction")
            activate(dismiss)
            tryVerify(() => fakeController.clearFeedbackCalls === 1)
        }
    }
}
