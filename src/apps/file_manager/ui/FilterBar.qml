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
                root.navigationController.setNameFilter(text)
                if (subfoldersToggle.checked)
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
