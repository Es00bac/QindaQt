import QtQuick

// Ready state: summary only. Feature screens arrive with later lanes.
Column {
    spacing: 16
    Text { text: "Connected"; color: "#6fd08c"; font.pixelSize: 28; font.bold: true }
    Text { text: link.desktopName; color: "white"; font.pixelSize: 22 }
    Text { text: "Sending battery and network status."; color: "#c8cdd6"; font.pixelSize: 16; wrapMode: Text.Wrap; width: parent.width }
    Button { objectName: "forgetButton"; text: "Forget desktop"; onClicked: link.forget() }
}
