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
    }
}
