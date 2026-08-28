// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// Bounded clipboard panel applet surface.
// The controller is the composed shell facade injected above QML; this file
// never imports the Clipboard service model or platform transports and owns
// no internal storage policy.
Item {
    id: root

    objectName: "clipboardApplet"
    implicitWidth: 380
    implicitHeight: content.implicitHeight

    property var controller: null

    readonly property bool showList:
        controller?.phaseText === "ready"
            || controller?.phaseText === "degraded"

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Clipboard")
    Accessible.description: {
        if (!controller)
            return qsTr("Clipboard controls are not connected")
        if (controller.phaseText === "loading")
            return qsTr("Clipboard history is loading")
        if (controller.phaseText === "locked")
            return qsTr("Clipboard history is hidden while locked")
        if (controller.phaseText === "disabled")
            return qsTr("Clipboard history is disabled")
        if (controller.phaseText === "unavailable")
            return qsTr("Clipboard is unavailable")
        return qsTr("Clipboard history and search items")
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Tokens.space["3"]

        // Header
        C.SectionHeader {
            id: sectionHeader
            objectName: "clipboardSectionHeader"
            Layout.fillWidth: true
            title: qsTr("Clipboard")
            description: showList && controller
                ? (controller.isSearchActive
                    ? qsTr("%1 search results").arg(controller.searchResultCount)
                    : qsTr("%1 items (%2)").arg(controller.entryCount).arg(controller.totalPayloadBytesFormatted))
                : ""
        }

        // Search & Filter Bar
        RowLayout {
            id: searchRow
            Layout.fillWidth: true
            visible: showList
            spacing: Tokens.space["2"]

            C.TextField {
                id: searchField
                objectName: "clipboardSearchField"
                Layout.fillWidth: true
                placeholderText: qsTr("Search clipboard history…")
                text: controller?.searchQuery ?? ""
                accessibleName: qsTr("Search clipboard history")
                onTextChanged: {
                    if (controller && text !== controller.searchQuery) {
                        controller.setSearchQuery(text)
                    }
                }
                Keys.onEscapePressed: {
                    searchField.text = ""
                    if (controller) {
                        controller.clearSearch()
                    }
                }
            }

            C.Button {
                id: clearSearchButton
                objectName: "clearSearchButton"
                visible: (controller?.isSearchActive ?? false) || (searchField.text.length > 0)
                text: qsTr("Clear")
                emphasized: false
                implicitWidth: 64
                implicitHeight: searchField.height
                accessibleDescription: qsTr("Clear search query")
                onClicked: {
                    searchField.text = ""
                    if (controller) {
                        controller.clearSearch()
                    }
                }
            }
        }

        // Action Toolbar
        RowLayout {
            id: toolbarRow
            Layout.fillWidth: true
            visible: showList && (controller?.entryCount ?? 0) > 0 && !controller?.isSearchActive
            spacing: Tokens.space["2"]

            C.Button {
                id: clearUnpinnedButton
                objectName: "clearUnpinnedButton"
                Layout.fillWidth: true
                text: qsTr("Clear Unpinned")
                emphasized: false
                destructive: false
                implicitHeight: 32
                accessibleDescription: qsTr("Clear all unpinned clipboard items")
                onClicked: {
                    if (controller) {
                        controller.clearHistory(true)
                    }
                }
            }

            C.Button {
                id: clearAllButton
                objectName: "clearAllButton"
                Layout.fillWidth: true
                text: qsTr("Clear All")
                emphasized: false
                destructive: true
                implicitHeight: 32
                accessibleDescription: qsTr("Clear all clipboard items including pinned")
                onClicked: {
                    if (controller) {
                        controller.clearHistory(false)
                    }
                }
            }
        }

        // State Cards & Notices
        C.StateCard {
            id: loadingState
            objectName: "clipboardLoadingState"
            Layout.fillWidth: true
            visible: controller?.phaseText === "loading"
            status: C.StateCard.Busy
            title: qsTr("Clipboard")
            message: qsTr("Clipboard history is loading…")
        }

        C.StateCard {
            id: lockedState
            objectName: "clipboardLockedState"
            Layout.fillWidth: true
            visible: controller?.phaseText === "locked"
            status: C.StateCard.Information
            title: qsTr("Clipboard Locked")
            message: controller?.phaseReasonText ?? qsTr("Clipboard content is hidden while session is locked.")
        }

        C.StateCard {
            id: disabledState
            objectName: "clipboardDisabledState"
            Layout.fillWidth: true
            visible: controller?.phaseText === "disabled"
            status: C.StateCard.Warning
            title: qsTr("Clipboard Disabled")
            message: controller?.phaseReasonText ?? qsTr("Clipboard history is currently disabled.")
        }

        C.DegradedNotice {
            id: unavailableNotice
            objectName: "clipboardUnavailableNotice"
            Layout.fillWidth: true
            visible: controller?.phaseText === "unavailable"
            title: qsTr("Clipboard is unavailable")
            reason: controller?.phaseReasonText ?? ""
        }

        C.DegradedNotice {
            id: degradedNotice
            objectName: "clipboardDegradedNotice"
            Layout.fillWidth: true
            visible: controller?.phaseText === "degraded"
            title: qsTr("Clipboard service is limited")
            reason: controller?.phaseReasonText ?? ""
        }

        C.StateCard {
            id: feedbackState
            objectName: "clipboardFeedbackState"
            Layout.fillWidth: true
            visible: controller?.feedbackPresent ?? false
            status: C.StateCard.Error
            title: qsTr("Clipboard Notice")
            message: controller?.feedback ?? ""
            actionText: qsTr("Dismiss")
            onActionTriggered: {
                if (controller) {
                    controller.clearFeedback()
                }
            }
        }

        // Empty state label
        C.Label {
            id: emptyLabel
            objectName: "clipboardEmptyLabel"
            Layout.fillWidth: true
            visible: showList && (controller?.entryCount ?? 0) === 0
            text: controller?.emptyReasonText ?? qsTr("Clipboard history is empty.")
            muted: true
        }

        // Entries List
        ListView {
            id: entriesList
            objectName: "clipboardEntriesList"
            Layout.fillWidth: true
            implicitHeight: Math.min(360, contentHeight)
            clip: true
            visible: showList && (controller?.entryCount ?? 0) > 0
            spacing: Tokens.space["2"]
            boundsBehavior: Flickable.StopAtBounds

            model: controller?.entryRows ?? []

            delegate: ClipboardEntryRow {
                width: entriesList.width
                entry: modelData
                controller: root.controller
                isCurrent: ListView.isCurrentItem
            }
        }
    }
}
