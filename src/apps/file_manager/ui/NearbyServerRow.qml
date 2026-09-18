// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One announced server on the Network hub. Extracted from
// NearbyServersSection.qml so the section stays within the source-shape
// budget with this delegate's own bindings counted.
Control {
    id: root

    // {key, name, host, scheme, port, address, subtitle, index}
    required property var service

    signal openRequested()
    signal saveRequested()

    padding: 10
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1, %2").arg(root.service.name).arg(root.service.subtitle)

    background: Rectangle {
        radius: 6
        color: root.palette.base
        border.color: root.palette.mid
    }

    contentItem: RowLayout {
        spacing: 10

        Image {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            source: "image://theme-icons/network-server"
            sourceSize: Qt.size(24, 24)
            Accessible.ignored: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.service.name
                elide: Text.ElideRight
                Accessible.ignored: true
            }
            Label {
                Layout.fillWidth: true
                text: root.service.subtitle
                color: root.palette.placeholderText
                elide: Text.ElideMiddle
                Accessible.ignored: true
            }
        }

        Button {
            objectName: "nearbyOpen_" + root.service.index
            text: qsTr("Open")
            Accessible.description: qsTr("Open %1").arg(root.service.key)
            onClicked: root.openRequested()
        }
        Button {
            objectName: "nearbySave_" + root.service.index
            text: qsTr("Save")
            Accessible.description: qsTr("Save %1 as a location").arg(root.service.key)
            onClicked: root.saveRequested()
        }
    }
}
