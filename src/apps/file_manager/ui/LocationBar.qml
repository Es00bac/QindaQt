// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as T
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Qinda

// Editable direct-path surface, swapped in over the breadcrumb by Ctrl+L or
// the toolbar toggle. Navigation itself stays in NavigationController; a
// rejected path keeps this bar active and surfaces through the existing
// status cards.
Rectangle {
    id: root

    required property var navigationController

    signal closed()

    function activate() {
        field.text = root.navigationController.currentPath
        field.forceActiveFocus()
        field.selectAll()
    }

    implicitHeight: 40
    color: Tokens.bg.base

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 0
        spacing: Tokens.space["2"]

        T.TextField {
            id: field
            objectName: "locationField"
            Layout.fillWidth: true
            color: Tokens.fg.default
            selectionColor: Tokens.accent.default
            selectedTextColor: Tokens.accent.fg
            placeholderTextColor: Tokens.fg.muted
            background: Rectangle { radius: 8; color: Tokens.bg.base; border.color: field.activeFocus ? Tokens.accent.default : Tokens.outline.divider }
            placeholderText: qsTr("Type a folder path")
            Accessible.name: qsTr("Location")

            onAccepted: {
                root.navigationController.navigateTo(text)
                if (root.navigationController.statusKey === "ready"
                        || root.navigationController.statusKey === "empty")
                    root.closed()
            }
            Keys.onEscapePressed: root.closed()
        }

        IconButton {
            iconName: "go-next"
            objectName: "locationGoButton"
            text: qsTr("Go")
            emphasized: false
            accessibleDescription: qsTr("Open the typed folder path")
            onClicked: field.accepted()
        }
    }
}
