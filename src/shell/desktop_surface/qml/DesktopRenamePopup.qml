// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Keyboard-capable rename surface; mutation remains in File Manager's
// identity-fenced boundary.
T.Popup {
    id: root
    property string entryId: ""
    signal renameRequested(string entryId, string newName)
    popupType: T.Popup.Window
    width: 360
    height: 132
    padding: 14
    modal: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    function begin(id, label) {
        entryId = id
        nameInput.text = label
        open()
        nameInput.forceActiveFocus(Qt.PopupFocusReason)
        nameInput.selectAll()
    }
    function submit() {
        const requested = nameInput.text.trim()
        if (requested.length === 0) return
        renameRequested(entryId, requested)
        close()
    }
    background: Rectangle {
        color: "#f020242a"
        radius: 8
        border.color: "#59616d"
    }
    contentItem: Column {
        spacing: 10
        Text { text: qsTr("Rename desktop item"); color: "#ffffff"; font.bold: true }
        Rectangle {
            width: parent.width
            height: 34
            radius: 4
            color: "#181b20"
            border.color: nameInput.activeFocus ? "#76a8ff" : "#59616d"
            TextInput {
                id: nameInput
                objectName: "desktopRenameInput"
                anchors.fill: parent
                anchors.margins: 7
                color: "#ffffff"
                selectionColor: "#3b74dd"
                selectedTextColor: "#ffffff"
                selectByMouse: true
                Keys.onReturnPressed: root.submit()
                Keys.onEnterPressed: root.submit()
            }
        }
        Row {
            anchors.right: parent.right
            spacing: 8
            Rectangle {
                width: 78; height: 28; radius: 4
                color: cancelInput.containsMouse ? "#414750" : "#30353d"
                Text { anchors.centerIn: parent; text: qsTr("Cancel"); color: "white" }
                MouseArea { id: cancelInput; anchors.fill: parent; hoverEnabled: true; onClicked: root.close() }
            }
            Rectangle {
                width: 78; height: 28; radius: 4
                color: renameInput.containsMouse ? "#5688e8" : "#3b74dd"
                Text { anchors.centerIn: parent; text: qsTr("Rename"); color: "white" }
                MouseArea { id: renameInput; anchors.fill: parent; hoverEnabled: true; onClicked: root.submit() }
            }
        }
    }
}
