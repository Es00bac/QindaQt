// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One saved network location on the Network hub. Presentation only: opening
// hands the address to NavigationController, so an unreachable server lands
// on the ordinary navigation state pane instead of a second error channel.
// Extracted from NetworkHub.qml so the hub stays within the source-shape
// budget with the delegate's own bindings counted.
Control {
    id: root

    // {id, name, url, showInPlaces, index}
    required property var location

    signal openRequested()
    signal removeRequested()

    padding: 10
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1, %2").arg(root.location.name).arg(root.location.url)

    background: Rectangle {
        radius: 6
        color: root.palette.alternateBase
        border.color: root.palette.mid
    }

    contentItem: RowLayout {
        spacing: 10

        Image {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            source: "image://theme-icons/folder-network"
            sourceSize: Qt.size(32, 32)
            Accessible.ignored: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.location.name
                font.bold: true
                elide: Text.ElideRight
                Accessible.ignored: true
            }
            Label {
                Layout.fillWidth: true
                text: root.location.url
                color: root.palette.placeholderText
                elide: Text.ElideMiddle
                Accessible.ignored: true
            }
        }

        Button {
            objectName: "networkLocationOpen_" + root.location.index
            text: qsTr("Open")
            Accessible.description: qsTr("Open %1").arg(root.location.url)
            onClicked: root.openRequested()
        }
        Button {
            objectName: "networkLocationRemove_" + root.location.index
            text: qsTr("Forget")
            Accessible.description: qsTr("Forget the saved location %1").arg(root.location.name)
            onClicked: root.removeRequested()
        }
    }
}
