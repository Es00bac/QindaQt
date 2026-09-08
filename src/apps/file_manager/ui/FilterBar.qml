// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as Qinda
import QindaQt.Tokens 1.0

Rectangle {
    id: root
    required property var navigationController
    signal closed()
    signal browseRequested()
    objectName: "folderFilterBar"
    color: Tokens.bg.raised
    implicitHeight: 48

    function activate() { field.forceActiveFocus(); field.selectAll() }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Tokens.space["3"]
        anchors.rightMargin: Tokens.space["2"]
        spacing: Tokens.space["2"]
        Qinda.TextField {
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
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Tokens.outline.divider }
}
