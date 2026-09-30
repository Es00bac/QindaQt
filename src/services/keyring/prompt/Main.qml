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
        Accessible.name: prompt.confirming ? qsTr("Delete this saved item?") : prompt.changing ? qsTr("Change keyring password") : prompt.revealing ? qsTr("Authenticate to reveal secret") : prompt.creation ? qsTr("Create keyring") : qsTr("Unlock keyring")
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
                    text: prompt.confirming ? qsTr("Delete this saved item?") : prompt.changing ? qsTr("Change keyring password") : prompt.revealing ? qsTr("Authenticate to reveal your secret") : prompt.creation ? qsTr("Create a keyring") : qsTr("Unlock your keyring")
                    font.bold: true
                }
                Tk.Label { Layout.fillWidth: true; text: prompt.label; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                Tk.TextField {
                    id: oldPassword
                    objectName: "keyringOldPasswordField"
                    Layout.fillWidth: true
                    visible: prompt.changing
                    echoMode: TextInput.Password
                    maximumLength: 4096
                    placeholderText: qsTr("Current password")
                    Accessible.name: qsTr("Current keyring password")
                    onAccepted: password.forceActiveFocus()
                }
                Tk.TextField {
                    id: password
                    visible: !prompt.confirming
                    objectName: "keyringPasswordField"
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    maximumLength: 4096
                    placeholderText: prompt.changing ? qsTr("New password") : qsTr("Password")
                    Accessible.name: qsTr("Keyring password")
                    Component.onCompleted: { if (prompt.changing) oldPassword.forceActiveFocus(); else forceActiveFocus() }
                    onAccepted: {
                        if (prompt.creation || prompt.changing) confirmation.forceActiveFocus()
                        else prompt.approve(text, "")
                    }
                }
                Tk.TextField {
                    id: confirmation
                    objectName: "keyringConfirmationField"
                    Layout.fillWidth: true
                    visible: prompt.creation || prompt.changing
                    echoMode: TextInput.Password
                    maximumLength: 4096
                    placeholderText: qsTr("Confirm password")
                    Accessible.name: qsTr("Confirm keyring password")
                    onAccepted: prompt.approve(password.text, text, oldPassword.text)
                }
                Tk.Label { Layout.fillWidth: true; text: prompt.status; visible: text.length > 0; wrapMode: Text.Wrap }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    Tk.Button { text: qsTr("Cancel"); onClicked: prompt.cancel() }
                    Tk.Button {
                        objectName: "keyringApproveButton"
                        Component.onCompleted: { if (prompt.confirming) forceActiveFocus() }
                        text: prompt.confirming ? qsTr("Delete") : prompt.changing ? qsTr("Change password") : prompt.revealing ? qsTr("Reveal") : prompt.creation ? qsTr("Create") : qsTr("Unlock")
                        enabled: prompt.confirming || password.length > 0 && (!(prompt.creation || prompt.changing) || confirmation.length > 0) && (!prompt.changing || oldPassword.length > 0)
                        onClicked: prompt.approve(password.text, confirmation.text, oldPassword.text)
                    }
                }
            }
        }
    }
    Connections {
        target: prompt
        function onClearFields() { oldPassword.clear(); password.clear(); confirmation.clear(); password.forceActiveFocus() }
    }
}
