import QtQuick

Window {
    visible: true
    color: "#14171c"
    title: "QindaLink"

    Item {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 72
        Loader {
            width: parent.width
            sourceComponent: link.phase === "confirm" ? pairing : link.phase === "ready" ? paired : connection
        }
    }
    Component { id: connection; ConnectionScreen { } }
    Component { id: pairing; PairingScreen { } }
    Component { id: paired; PairedScreen { } }
}
