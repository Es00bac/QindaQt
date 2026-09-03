// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import QindaQt.Shell.ClipboardApplet 1.0 as ClipboardComponents
import QindaQt.Shell.ClipboardApplet.Tests 1.0 as Harness

// Real-event interactive proof for the Clipboard applet surface. This file
// runs under the compiled harness (qml_interactive_main.cpp) so the QST-1
// token singleton carries a published theme and the scene has real layout;
// every interaction below delivers genuine Qt pointer events through the
// scene rather than invoking handlers directly.
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
        property string feedbackStatus: "error"

        property int selectCalls: 0
        property int deleteCalls: 0
        property int togglePinCalls: 0
        property int clearCalls: 0
        property bool lastClearUnpinnedOnly: false
        // Exact-argument capture: intent identity assertions compare the
        // forwarded (generation, serial) pair, never just a call count.
        property var lastSelectArgs: null
        property var lastDeleteArgs: null
        property var lastTogglePinArgs: null

        function selectEntry(gen, ser) {
            ++selectCalls
            lastSelectArgs = [gen, ser]
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
            searchQuery = ""
            isSearchActive = false
        }
    }

    Component {
        id: appletComponent

        ClipboardComponents.ClipboardApplet {
            width: 380
            controller: fakeController
        }
    }

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

    TestCase {
        name: "ClipboardAppletInteractiveTests"
        // Force the harness singleton onto this engine (publishes the
        // QST-1 theme) before any applet component is created.
        readonly property bool harnessReady: Harness.Harness.ready
        when: windowShown

        function init() {
            fakeController.phaseText = "ready"
            fakeController.entryRows = []
            fakeController.entryCount = 0
            fakeController.searchQuery = ""
            fakeController.isSearchActive = false
            fakeController.selectCalls = 0
            fakeController.deleteCalls = 0
            fakeController.togglePinCalls = 0
            fakeController.clearCalls = 0
            fakeController.clipboardWriteGranted = true
            fakeController.lastSelectArgs = null
            fakeController.lastDeleteArgs = null
            fakeController.lastTogglePinArgs = null
        }

        // Capability honesty: with clipboard.write denied in the ready phase,
        // mutating controls are disabled and real clicks dispatch nothing,
        // while the list stays browsable.
        function test_writeDenialDisablesMutationsHonestly() {
            fakeController.clipboardWriteGranted = false
            fakeController.entryRows = [makeEntry(14, { preview: "read only" })]
            fakeController.entryCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)
            compare(list.visible, true)

            var pin = findChild(applet, "pinButton")
            verify(pin !== null)
            compare(pin.enabled, false)
            mouseClick(pin)
            compare(fakeController.togglePinCalls, 0)

            var del = findChild(applet, "deleteButton")
            compare(del.enabled, false)
            mouseClick(del)
            compare(fakeController.deleteCalls, 0)

            var clearAll = findChild(applet, "clearAllButton")
            verify(clearAll !== null)
            compare(clearAll.enabled, false)
        }

        // AGENT-GUARD (P1 regression): real pointer events must reach the
        // Pin and Delete buttons through the full scene stack — the row's
        // full-body selection MouseArea must never cover them. mouseClick
        // posts genuine Qt mouse events at the item centers. Each intent must
        // forward the displayed entry's exact (generation, serial) identity:
        // the pre-repair row forwarded `undefined` as the Pin serial.
        function test_realPointerClicksReachActionButtons() {
            fakeController.entryRows = [makeEntry(9, { generation: 4, preview: "pointer target" })]
            fakeController.entryCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            verify(list !== null)
            tryVerify(() => list.count === 1)

            var pin = findChild(applet, "pinButton")
            verify(pin !== null)
            verify(pin.width > 0 && pin.height > 0)
            mouseClick(pin)
            compare(fakeController.togglePinCalls, 1)
            compare(fakeController.lastTogglePinArgs, [4, 9])
            compare(fakeController.selectCalls, 0)
            compare(fakeController.deleteCalls, 0)

            var del = findChild(applet, "deleteButton")
            verify(del !== null)
            verify(del.width > 0 && del.height > 0)
            mouseClick(del)
            compare(fakeController.deleteCalls, 1)
            compare(fakeController.lastDeleteArgs, [4, 9])
            compare(fakeController.togglePinCalls, 1)
            compare(fakeController.selectCalls, 0)
        }

        function test_realPointerRowBodySelects() {
            fakeController.entryRows = [makeEntry(10)]
            fakeController.entryCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)

            var row = findChild(applet, "clipboardEntryRow")
            verify(row !== null)
            verify(row.width > 0 && row.height > 0)
            // Click the badge area at the row's left edge: far from the
            // action buttons, so the selection MouseArea must receive it.
            mouseClick(row, 8, row.height / 2)
            compare(fakeController.selectCalls, 1)
            compare(fakeController.lastSelectArgs, [1, 10])
            compare(fakeController.togglePinCalls, 0)
            compare(fakeController.deleteCalls, 0)
        }

        // P1 regression: clipboard.write denial keeps metadata SEARCH enabled
        // (search is a read path) while mutating controls stay disabled.
        function test_readOnlyGrantKeepsSearchEnabled() {
            fakeController.clipboardWriteGranted = false
            fakeController.entryRows = [makeEntry(15, { preview: "read only searchable" })]
            fakeController.entryCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)

            var searchField = findChild(applet, "clipboardSearchField")
            verify(searchField !== null)
            compare(searchField.enabled, true)

            // Typing reaches the controller: read-only search is live.
            searchField.forceActiveFocus()
            searchField.text = "read only"
            compare(fakeController.searchQuery, "read only")

            // Mutation controls remain honestly disabled under write denial.
            var pin = findChild(applet, "pinButton")
            verify(pin !== null)
            compare(pin.enabled, false)
        }

        // P2 regression: degraded keeps read-only browsing; every mutating
        // control is disabled rather than dead-looking, and real clicks on
        // disabled controls dispatch nothing.
        function test_degradedStateDisablesActionsHonestly() {
            fakeController.phaseText = "degraded"
            fakeController.entryRows = [makeEntry(11, { preview: "degraded browsing" })]
            fakeController.entryCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)
            compare(list.visible, true)

            var pin = findChild(applet, "pinButton")
            verify(pin !== null)
            compare(pin.available, false)
            compare(pin.enabled, false)
            mouseClick(pin)
            compare(fakeController.togglePinCalls, 0)

            var del = findChild(applet, "deleteButton")
            compare(del.enabled, false)
            mouseClick(del)
            compare(fakeController.deleteCalls, 0)

            var row = findChild(applet, "clipboardEntryRow")
            verify(row !== null)
            compare(row.actionsAvailable, false)
            mouseClick(row, 8, row.height / 2)
            compare(fakeController.selectCalls, 0)

            var searchField = findChild(applet, "clipboardSearchField")
            verify(searchField !== null)
            compare(searchField.enabled, false)

            var clearAll = findChild(applet, "clearAllButton")
            verify(clearAll !== null)
            compare(clearAll.enabled, false)

            var caps = findChild(applet, "clipboardDegradedCapabilitiesLabel")
            verify(caps !== null)
            compare(caps.visible, true)
        }

        // P2 regression: in-flight mutations present as busy controls that
        // cannot be double-triggered by real clicks, and the row announces
        // the pending operation to assistive technology.
        function test_pendingRowsShowBusyControls() {
            // The accessible-name pending phrase mirrors the C++ projection
            // contract (covered by qindaqt.clipboard-applet-model); the fake
            // carries it explicitly because the fake bypasses the projector.
            fakeController.entryRows = [makeEntry(12, {
                pending: true,
                accessibleName: "Entry 1: text/plain, operation pending, preview: entry 12"
            })]
            fakeController.entryCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)

            var pin = findChild(applet, "pinButton")
            verify(pin !== null)
            compare(pin.busy, true)
            compare(pin.enabled, false)
            mouseClick(pin)
            compare(fakeController.togglePinCalls, 0)

            var row = findChild(applet, "clipboardEntryRow")
            verify(row !== null)
            verify(row.entry.pending === true)
            verify(row.Accessible.name.indexOf("operation pending") !== -1)
        }

        // P2 regression (token sanity): with the harness theme published the
        // scene lays out with real, finite token geometry.
        function test_tokensProduceRealLayout() {
            fakeController.entryRows = [makeEntry(13)]
            fakeController.entryCount = 1

            var applet = createTemporaryObject(appletComponent, testRoot)
            verify(applet !== null)
            var list = findChild(applet, "clipboardEntriesList")
            tryVerify(() => list.count === 1)

            var row = findChild(applet, "clipboardEntryRow")
            verify(row.height >= 56)
            verify(isFinite(row.height) && row.height > 0)
        }
    }
}
