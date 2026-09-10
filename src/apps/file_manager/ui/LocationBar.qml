// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Editable direct-path surface, swapped in over the breadcrumb by Ctrl+L or
// the toolbar toggle. Navigation itself stays in NavigationController; a
// rejected path keeps this bar active and surfaces through the existing
// status banners.
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
    color: "transparent"

    RowLayout {
        anchors.fill: parent
        spacing: 8

        TextField {
            id: field
            objectName: "locationField"
            Layout.fillWidth: true
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
            Accessible.description: qsTr("Open the typed folder path")
            onClicked: field.accepted()
        }
    }
}
