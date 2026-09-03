// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

QtObject {
    property string phaseText: "ready"
    property string phaseReasonText: ""
    property bool isLocked: false
    property bool isHistoryEnabled: true
    property bool clipboardReadGranted: true
    property bool clipboardWriteGranted: true
    property var entryRows: []
    property int entryCount: 0
    property int pinnedCount: 0
    property int unpinnedCount: 0
    property int totalPayloadBytes: 0
    property string totalPayloadBytesFormatted: "0 B"
    property bool isSearchActive: false
    property string searchQuery: ""
    property int searchResultCount: 0
    property bool searchTruncated: false
    property string emptyReasonText: "Clipboard history is empty."
    property int pendingOperationCount: 0
    property bool feedbackPresent: false
    property string feedback: ""
    property string feedbackStatus: "error"

    function selectEntry() { return true }
    function deleteEntry() { return true }
    function togglePin() { return true }
    function clearHistory() { return true }
    function setSearchQuery(query) { searchQuery = query }
    function clearSearch() { searchQuery = "" }
    function clearFeedback() {}
}
