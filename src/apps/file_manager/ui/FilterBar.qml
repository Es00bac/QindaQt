// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root
    required property var navigationController
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
            placeholderText: qsTr("Filter this folder by name…")
            Accessible.name: qsTr("Filter this folder by name")
            Accessible.description: qsTr("Matches names in this folder. Escape clears the filter.")
            maximumLength: root.navigationController.maximumNameFilterLength
            text: root.navigationController.nameFilter
            onTextEdited: root.navigationController.setNameFilter(text)
            onAccepted: root.browseRequested()
            Keys.onEscapePressed: root.closed()
        }
        IconButton {
            objectName: "closeFolderFilterButton"
            iconName: "window-close"
            text: qsTr("Clear and close filter (Escape)")
            onClicked: root.closed()
        }
    }
}
