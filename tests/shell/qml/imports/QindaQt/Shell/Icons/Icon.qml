// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root

    required property string name
    property int size: 20
    property color color: "#f0f4f1"
    property bool symbolic: true
    property string fallbackText: "?"
    readonly property bool resolved: name.length > 0

    implicitWidth: size
    implicitHeight: size

    Item {
        objectName: "iconImage"
        visible: root.resolved
        property url source: root.resolved ? "image://fixture/" + root.name : ""
    }

    Rectangle {
        objectName: "placeholderTile"
        anchors.fill: parent
        visible: !root.resolved
        color: "transparent"

        Text {
            anchors.centerIn: parent
            text: parent.parent.fallbackText.slice(0, 1)
            color: parent.parent.color
        }
    }
}
