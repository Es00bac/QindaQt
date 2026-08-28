// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/clipboard_applet/qml" as ClipboardComponents

Item {
    id: testRoot
    width: 400
    height: 600

    QtObject {
        id: fakeController

        property string phaseText: "ready"
        property string phaseReasonText: ""
        property bool isLocked: false
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
        property string feedbackStatus: "error"

        property int selectCalls: 0
        property int deleteCalls: 0
        property int togglePinCalls: 0
        property int clearCalls: 0
        property bool lastClearUnpinnedOnly: false
        property int searchCalls: 0
        property string lastSearchQuery: ""
        property int clearSearchCalls: 0
        property int clearFeedbackCalls: 0

        function selectEntry(gen, ser) {
            ++selectCalls
            return true
        }

        function deleteEntry(gen, ser) {
            ++deleteCalls
            return true
        }

        function togglePin(gen, ser) {
            ++togglePinCalls
            return true
        }

        function clearHistory(unpinnedOnly) {
            ++clearCalls
            lastClearUnpinnedOnly = unpinnedOnly
            return true
        }

        function setSearchQuery(query) {
            ++searchCalls
            lastSearchQuery = query
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

    Component {
        id: appletComponent

        ClipboardComponents.ClipboardApplet {
            width: 380
            controller: fakeController
        }
    }

    TestCase {
        name: "ClipboardAppletTests"
        when: windowShown

        function init() {
            fakeController.phaseText = "ready"
            fakeController.phaseReasonText = ""
            fakeController.isLocked = false
            fakeController.isHistoryEnabled = true
            fakeController.entryRows = []
            fakeController.entryCount = 0
            fakeController.pinnedCount = 0
            fakeController.unpinnedCount = 0
            fakeController.totalPayloadBytes = 0
            fakeController.totalPayloadBytesFormatted = "0 B"
            fakeController.isSearchActive = false
            fakeController.searchQuery = ""
            fakeController.searchResultCount = 0
            fakeController.searchTruncated = false
            fakeController.emptyReasonText = "Clipboard history is empty."
            fakeController.feedbackPresent = false
            fakeController.feedback = ""

            fakeController.selectCalls = 0
            fakeController.deleteCalls = 0
            fakeController.togglePinCalls = 0
            fakeController.clearCalls = 0
            fakeController.searchCalls = 0
            fakeController.clearSearchCalls = 0
            fakeController.clearFeedbackCalls = 0
        }

        function test_loadingState() {
            fakeController.phaseText = "loading"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var loading = findChild(applet, "clipboardLoadingState")
            verify(loading !== null)
            compare(loading.visible, true)

            var list = findChild(applet, "clipboardEntriesList")
            compare(list.visible, false)
        }

        function test_lockedState() {
            fakeController.phaseText = "locked"
            fakeController.isLocked = true
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var locked = findChild(applet, "clipboardLockedState")
            verify(locked !== null)
            compare(locked.visible, true)

            var list = findChild(applet, "clipboardEntriesList")
            compare(list.visible, false)
        }

        function test_disabledState() {
            fakeController.phaseText = "disabled"
            fakeController.isHistoryEnabled = false
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var disabled = findChild(applet, "clipboardDisabledState")
            verify(disabled !== null)
            compare(disabled.visible, true)
        }

        function test_unavailableAndDegradedState() {
            fakeController.phaseText = "unavailable"
            fakeController.phaseReasonText = "Daemon offline"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var unavail = findChild(applet, "clipboardUnavailableNotice")
            verify(unavail !== null)
            compare(unavail.visible, true)

            fakeController.phaseText = "degraded"
            var degraded = findChild(applet, "clipboardDegradedNotice")
            verify(degraded !== null)
            compare(degraded.visible, true)
        }

        function test_emptyState() {
            fakeController.phaseText = "ready"
            fakeController.entryRows = []
            fakeController.entryCount = 0
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var empty = findChild(applet, "clipboardEmptyLabel")
            verify(empty !== null)
            compare(empty.visible, true)
        }

        function test_readyWithEntries() {
            fakeController.phaseText = "ready"
            fakeController.entryRows = [
                {
                    generation: 1,
                    serial: 1,
                    idString: "1:1",
                    preview: "First test item",
                    previewTruncated: false,
                    sourceLabel: "Editor",
                    pinned: true,
                    formatsSummary: "text/plain (15 B)",
                    primaryMediaType: "text/plain",
                    isText: true,
                    isImage: false,
                    isUriList: false,
                    totalBytes: 15,
                    admittedTick: 100,
                    lastUsedTick: 100,
                    accessibleName: "Entry 1: text/plain, pinned",
                    accessibleDescription: "text/plain; 15 B",
                    pending: false
                }
            ]
            fakeController.entryCount = 1
            fakeController.pinnedCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var list = findChild(applet, "clipboardEntriesList")
            verify(list !== null)
            compare(list.visible, true)
            compare(list.count, 1)
        }

        function test_searchInteraction() {
            fakeController.phaseText = "ready"
            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var searchField = findChild(applet, "clipboardSearchField")
            verify(searchField !== null)
            searchField.text = "query"
            compare(fakeController.searchCalls, 1)
            compare(fakeController.lastSearchQuery, "query")

            var clearBtn = findChild(applet, "clearSearchButton")
            verify(clearBtn !== null)
            clearBtn.clicked()
            compare(fakeController.clearSearchCalls, 1)
        }

        function test_clearButtons() {
            fakeController.phaseText = "ready"
            fakeController.entryCount = 3
            fakeController.pinnedCount = 1
            fakeController.entryRows = [
                { generation: 1, serial: 1, pinned: true, preview: "a", formatsSummary: "", accessibleName: "", accessibleDescription: "", isText: true, isImage: false, isUriList: false, sourceLabel: "", pending: false },
                { generation: 1, serial: 2, pinned: false, preview: "b", formatsSummary: "", accessibleName: "", accessibleDescription: "", isText: true, isImage: false, isUriList: false, sourceLabel: "", pending: false },
                { generation: 1, serial: 3, pinned: false, preview: "c", formatsSummary: "", accessibleName: "", accessibleDescription: "", isText: true, isImage: false, isUriList: false, sourceLabel: "", pending: false }
            ]

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var clearUnpinned = findChild(applet, "clearUnpinnedButton")
            verify(clearUnpinned !== null)
            clearUnpinned.clicked()
            compare(fakeController.clearCalls, 1)
            compare(fakeController.lastClearUnpinnedOnly, true)

            var clearAll = findChild(applet, "clearAllButton")
            verify(clearAll !== null)
            clearAll.clicked()
            compare(fakeController.clearCalls, 2)
            compare(fakeController.lastClearUnpinnedOnly, false)
        }

        function test_feedbackDismiss() {
            fakeController.phaseText = "ready"
            fakeController.feedbackPresent = true
            fakeController.feedback = "An error occurred"

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var feedbackCard = findChild(applet, "clipboardFeedbackState")
            verify(feedbackCard !== null)
            compare(feedbackCard.visible, true)

            fakeController.clearFeedback()
            compare(fakeController.clearFeedbackCalls, 1)
        }
    }
}
