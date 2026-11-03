// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// A password field that never teaches the keyboard: sensitive-data, no prediction, no auto-capital,
// hidden by default. The eye button reveals and re-hides after 10 s; leaving the page clears the text
// because the owner view is destroyed. AGENT-GUARD: keep the three input hints; without them the IME
// can store the value in its dictionary.
Tk.Flex {
    id: root

    property alias text: field.text
    property string placeholder: ""
    property bool reveal: false

    direction: Tk.Flex.Row
    align: Tk.Flex.Center
    gap: Tk.Theme.space.xs

    function clear() { field.text = ""; reveal = false }

    Tk.TextField {
        id: field
        objectName: "secretInput"
        Tk.Flex.grow: 1
        Tk.Flex.basis: 0
        mono: true
        placeholderText: root.placeholder
        echoMode: root.reveal ? TextInput.Normal : TextInput.Password
        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            | (root.reveal ? 0 : Qt.ImhHiddenText)
    }
    Tk.IconButton {
        objectName: "secretReveal"
        iconName: root.reveal ? "eye-off" : "eye"
        tooltip: root.reveal ? qsTr("Hide") : qsTr("Show for 10 seconds")
        onClicked: root.reveal = !root.reveal
    }
    Timer {
        interval: 10000
        running: root.reveal
        onTriggered: root.reveal = false
    }
    Component.onDestruction: field.text = ""
}
