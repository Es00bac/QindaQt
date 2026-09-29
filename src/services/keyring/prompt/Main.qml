// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaTK as Tk

Window {
    id: root
    objectName: "keyringPromptWindow"
    visible: false
    color: "transparent"
    title: qsTr("Passwords & Keys")
    onClosing: prompt.cancel()
    Rectangle {
        anchors.fill: parent
        color: Tk.Theme.color.bg
        focus: true
        Keys.onEscapePressed: prompt.cancel()
        Accessible.role: Accessible.Dialog
        Accessible.name: prompt.creation ? qsTr("Create keyring") : qsTr("Unlock keyring")
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - Tk.Theme.space.lg * 2, 420)
            height: content.implicitHeight + Tk.Theme.space.lg * 2
            color: Tk.Theme.color.panel
            ColumnLayout {
                id: content
                anchors.fill: parent
                anchors.margins: Tk.Theme.space.lg
                spacing: Tk.Theme.space.md
                Tk.Label {
                    Layout.fillWidth: true
                    text: prompt.creation ? qsTr("Create a keyring") : qsTr("Unlock your keyring")
                    font.bold: true
                }
                Tk.Label { Layout.fillWidth: true; text: prompt.label; wrapMode: Text.Wrap }
                Tk.TextField {
                    id: password
                    objectName: "keyringPasswordField"
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    maximumLength: 4096
                    placeholderText: qsTr("Password")
                    Accessible.name: qsTr("Keyring password")
                    Component.onCompleted: forceActiveFocus()
                    onAccepted: {
                        if (prompt.creation) confirmation.forceActiveFocus()
                        else prompt.approve(text, "")
                    }
                }
                Tk.TextField {
                    id: confirmation
                    objectName: "keyringConfirmationField"
                    Layout.fillWidth: true
                    visible: prompt.creation
                    echoMode: TextInput.Password
                    maximumLength: 4096
                    placeholderText: qsTr("Confirm password")
                    Accessible.name: qsTr("Confirm keyring password")
                    onAccepted: prompt.approve(password.text, text)
                }
                Tk.Label { Layout.fillWidth: true; text: prompt.status; visible: text.length > 0; wrapMode: Text.Wrap }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    Tk.Button { text: qsTr("Cancel"); onClicked: prompt.cancel() }
                    Tk.Button {
                        objectName: "keyringApproveButton"
                        text: prompt.creation ? qsTr("Create") : qsTr("Unlock")
                        enabled: password.length > 0 && (!prompt.creation || confirmation.length > 0)
                        onClicked: prompt.approve(password.text, confirmation.text)
                    }
                }
            }
        }
    }
    Connections {
        target: prompt
        function onClearFields() { password.clear(); confirmation.clear(); password.forceActiveFocus() }
    }
}
