import QtQuick

// Shown while not pairing: state, desktop name, and the actions that apply.
Column {
    spacing: 16
    Text { text: "QindaLink"; color: "white"; font.pixelSize: 28; font.bold: true }
    Text { text: "Status: " + link.phase; color: "#9fb4d8"; font.pixelSize: 20 }
    Text {
        visible: link.desktopName !== ""
        text: "Desktop: " + link.desktopName
        color: "white"; font.pixelSize: 20
    }
    Text { text: link.message; color: "#c8cdd6"; font.pixelSize: 16; wrapMode: Text.Wrap; width: parent.width }
    Button {
        objectName: "pairButton"
        visible: link.phase === "connected"
        text: "Pair with this desktop"
        primary: true
        onClicked: link.pair()
    }
    Button {
        objectName: "pairAgainButton"
        visible: link.phase === "stale"
        text: "Pair again"
        primary: true
        onClicked: link.pairAgain()
    }
    Button {
        objectName: "forgetButton"
        visible: link.paired
        text: "Forget desktop"
        onClicked: link.forget()
    }
}
