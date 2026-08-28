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

        property int selectCalls: 0
        property int deleteCalls: 0
        property int clearSearchCalls: 0

        function selectEntry(gen, ser) {
            ++selectCalls
            return true
        }

        function deleteEntry(gen, ser) {
            ++deleteCalls
            return true
        }

        function clearSearch() {
            ++clearSearchCalls
            searchQuery = ""
            isSearchActive = false
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
        when: windowShown

        function init() {
            fakeController.selectCalls = 0
            fakeController.deleteCalls = 0
            fakeController.clearSearchCalls = 0
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
    }
}
