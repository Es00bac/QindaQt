// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root
    required property var navigationController
    required property var searchController
    signal closed()
    signal browseRequested()
    objectName: "folderFilterBar"
    implicitHeight: 48
    leftPadding: 12
    rightPadding: 8

    function activate() { field.forceActiveFocus(); field.selectAll() }

    function stopSearch() {
        if (searchDebounce) searchDebounce.stop()
        if (root.searchController) root.searchController.cancel()
    }

    // AGENT-GUARD: A hidden filter or a new folder must not inherit a queued
    // recursive search. Its timer otherwise uses the new folder at delivery.
    onVisibleChanged: if (!visible) stopSearch()
    readonly property string searchPath: navigationController ? navigationController.currentPath : ""
    onSearchPathChanged: stopSearch()

    background: Rectangle {
        color: root.palette.window
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.palette.mid }
    }

    contentItem: RowLayout {
        spacing: 8
        TextField {
            id: field
            objectName: "folderFilterField"
            Layout.fillWidth: true
            placeholderText: subfoldersToggle.checked
                ? qsTr("Search this folder and subfolders…")
                : qsTr("Filter this folder by name…")
            Accessible.name: subfoldersToggle.checked
                ? qsTr("Search this folder and subfolders")
                : qsTr("Filter this folder by name")
            Accessible.description: qsTr("Matches names in this folder. Escape clears the filter.")
            maximumLength: root.navigationController.maximumNameFilterLength
            text: root.navigationController.nameFilter
            onTextEdited: {
                root.stopSearch()
                root.navigationController.setNameFilter(text)
                if (text.trim().length === 0 && root.navigationController.guestListingActive)
                    root.navigationController.clearGuestListing()
                else if (subfoldersToggle.checked && subfoldersToggle.enabled)
                    searchDebounce.restart()
            }
            onAccepted: root.browseRequested()
            Keys.onEscapePressed: root.closed()
        }

        // Debounce keeps every keystroke from restarting the bounded worker
        // search; the final state always wins.
        Timer {
            id: searchDebounce
            interval: 300
            onTriggered: root.searchController.startSearch(
                root.navigationController.currentPath, field.text,
                root.navigationController.showHidden)
        }

        CheckBox {
            id: subfoldersToggle
            objectName: "filterSubfoldersToggle"
            // ADR-0262/ADR-0272: the Applications and Recents places have no
            // subfolders to search.
            enabled: !root.navigationController.applicationsPlace
                     && !root.navigationController.recentsPlace
            text: qsTr("Subfolders")
            Accessible.name: qsTr("Include subfolders (recursive search)")
            onToggled: {
                searchDebounce.stop()
                if (checked) {
                    if (field.text.length > 0)
                        root.searchController.startSearch(
                            root.navigationController.currentPath, field.text,
                            root.navigationController.showHidden)
                } else {
                    root.searchController.cancel()
                    if (root.navigationController.guestListingActive)
                        root.navigationController.clearGuestListing()
                }
            }
        }

        IconButton {
            objectName: "closeFolderFilterButton"
            iconName: "window-close"
            text: qsTr("Clear and close filter (Escape)")
            onClicked: root.closed()
        }
    }
}
