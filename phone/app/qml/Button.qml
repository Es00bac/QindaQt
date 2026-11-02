import QtQuick

// Plain button; QindaTK replaces this file when the screens move over.
Rectangle {
    id: root
    property string text
    property bool primary: false
    signal clicked
    implicitWidth: 260
    implicitHeight: 56
    radius: 12
    color: primary ? "#2d6cdf" : "#2b2f38"
    border.color: "#4a5160"
    Text {
        anchors.centerIn: parent
        text: root.text
        color: "white"
        font.pixelSize: 18
    }
    MouseArea { anchors.fill: parent; onClicked: root.clicked() }
}
