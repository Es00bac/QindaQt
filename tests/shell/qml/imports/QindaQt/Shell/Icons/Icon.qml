// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    required property string name
    property int size: 20
    property color color: "#f0f4f1"
    property bool symbolic: true
    property string fallbackText: "?"
    readonly property bool resolved: false

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        objectName: "placeholderTile"
        anchors.fill: parent
        color: "transparent"

        Text {
            anchors.centerIn: parent
            text: parent.parent.fallbackText.slice(0, 1)
            color: parent.parent.color
        }
    }
}
