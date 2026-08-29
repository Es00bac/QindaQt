// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../../../src/shell/clipboard_applet/qml" as ClipboardComponents

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
        when: windowShown

        function init() {
            fakeController.phaseText = "ready"
            fakeController.entryRows = []
            fakeController.entryCount = 0
            fakeController.selectCalls = 0
            fakeController.deleteCalls = 0
            fakeController.togglePinCalls = 0
            fakeController.clearCalls = 0
        }

        // AGENT-GUARD (P1 regression): real pointer events must reach the
        // Pin and Delete buttons through the full scene stack — the row's
        // full-body selection MouseArea must never cover them. mouseClick
        // posts genuine Qt mouse events at the item centers.
        function test_realPointerClicksReachActionButtons() {
            fakeController.entryRows = [makeEntry(9, { preview: "pointer target" })]
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
            compare(fakeController.selectCalls, 0)
            compare(fakeController.deleteCalls, 0)

            var del = findChild(applet, "deleteButton")
            verify(del !== null)
            verify(del.width > 0 && del.height > 0)
            mouseClick(del)
            compare(fakeController.deleteCalls, 1)
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
            compare(fakeController.togglePinCalls, 0)
            compare(fakeController.deleteCalls, 0)
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
            fakeController.entryRows = [makeEntry(12, { pending: true })]
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
