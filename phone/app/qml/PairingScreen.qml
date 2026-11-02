import QtQuick

// Code comparison. Accept only after the same six digits show on the desktop.
Column {
    spacing: 20
    Text { text: "Pair with " + link.desktopName; color: "white"; font.pixelSize: 22 }
    Text {
        objectName: "pairCode"
        text: link.code
        color: "white"; font.pixelSize: 56; font.bold: true; font.family: "monospace"
    }
    Text { text: "Do these digits match the desktop?"; color: "#c8cdd6"; font.pixelSize: 16 }
    Button { objectName: "acceptButton"; text: "Accept"; primary: true; onClicked: link.accept() }
    Button { objectName: "rejectButton"; text: "Reject"; onClicked: link.reject() }
}
