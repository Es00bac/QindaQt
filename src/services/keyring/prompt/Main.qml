// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QindaTK as Tk

Window {
    id: root
    objectName: "keyringPromptWindow"
    visible: false
    color: "transparent"
    title: qsTr("QindaQt — Keyring access request")
    onClosing: prompt.cancel()
    function focusInitialField() {
        if (!prompt.reviewed || prompt.confirming) denyButton.forceActiveFocus()
        else if (prompt.changing) oldPassword.forceActiveFocus()
        else password.forceActiveFocus()
    }
    onActiveChanged: { if (active) focusInitialField() }
    component RequestText: Tk.Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    Rectangle {
        anchors.fill: parent
        color: Tk.Theme.color.bg
        focus: true
        Keys.onEscapePressed: prompt.cancel()
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Review keyring access request")
        Accessible.description: prompt.requester + ". " + prompt.operation + " " + prompt.scope
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - Tk.Theme.space.lg * 2, 720)
            height: Math.min(parent.height - Tk.Theme.space.lg * 2,
                             content.implicitHeight + footer.implicitHeight + Tk.Theme.space.lg * 3)
            color: Tk.Theme.color.panel
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Tk.Theme.space.lg
                spacing: Tk.Theme.space.md
                Controls.ScrollView {
                    id: scroll
                    objectName: "keyringRequestScroll"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        id: content
                        width: scroll.availableWidth
                        spacing: Tk.Theme.space.md
                        RequestText {
                            text: qsTr("Review keyring access request")
                            font.bold: true
                        }
                        RequestText { text: qsTr("Requesting program"); font.bold: true }
                        RequestText {
                            objectName: "keyringRequester"
                            text: prompt.requester
                            font.bold: true
                        }
                        RequestText { objectName: "keyringRequesterDetails"; text: prompt.requesterDetails }
                        RequestText {
                            objectName: "keyringIdentityWarning"
                            visible: prompt.requesterKnown && !prompt.executableKnown
                            text: qsTr("The operating system did not expose this program's executable. Protected applications can hide it. Its program name and safety have not been verified.")
                            font.bold: true
                        }
                        RequestText { text: qsTr("Parent processes (observed context)"); font.bold: true }
                        RequestText { objectName: "keyringParentContext"; text: prompt.parentContext }
                        RequestText {
                            text: qsTr("These details come from the session bus and operating system. They do not verify the program's safety or the agent's intent.")
                        }
                        RequestText { text: qsTr("Why this appeared"); font.bold: true }
                        RequestText { objectName: "keyringOperation"; text: prompt.operation }
                        RequestText {
                            objectName: "keyringTaskReason"
                            text: qsTr("Agent task / reason: not supplied. Continue only if this request matches an action you intended.")
                        }
                        RequestText {
                            visible: prompt.application.length > 0
                            text: qsTr("Application reported by the portal: %1").arg(prompt.application)
                        }
                        RequestText {
                            objectName: "keyringTarget"
                            text: (prompt.confirming ? qsTr("Saved item label: %1") : qsTr("Keyring label: %1")).arg(prompt.label)
                        }
                        RequestText { text: qsTr("What approval permits"); font.bold: true }
                        RequestText { objectName: "keyringAccessScope"; text: prompt.scope }
                        Tk.TextField {
                            id: oldPassword
                            objectName: "keyringOldPasswordField"
                            Layout.fillWidth: true
                            visible: prompt.reviewed && prompt.changing
                            enabled: visible
                            echoMode: TextInput.Password
                            maximumLength: 4096
                            placeholderText: qsTr("Current password")
                            Accessible.name: qsTr("Current keyring password")
                            onActiveFocusChanged: {
                                if (activeFocus) scroll.contentItem.contentY = Math.max(0, y + height - scroll.availableHeight)
                            }
                            onAccepted: password.forceActiveFocus()
                        }
                        Tk.TextField {
                            id: password
                            objectName: "keyringPasswordField"
                            Layout.fillWidth: true
                            visible: prompt.reviewed && !prompt.confirming
                            enabled: visible
                            echoMode: TextInput.Password
                            maximumLength: 4096
                            placeholderText: prompt.changing ? qsTr("New password") : qsTr("Keyring password")
                            Accessible.name: qsTr("Keyring password")
                            onActiveFocusChanged: {
                                if (activeFocus) scroll.contentItem.contentY = Math.max(0, y + height - scroll.availableHeight)
                            }
                            onAccepted: {
                                if (prompt.creation || prompt.changing) confirmation.forceActiveFocus()
                                else prompt.approve(text, "")
                            }
                        }
                        Tk.TextField {
                            id: confirmation
                            objectName: "keyringConfirmationField"
                            Layout.fillWidth: true
                            visible: prompt.reviewed && (prompt.creation || prompt.changing)
                            enabled: visible
                            echoMode: TextInput.Password
                            maximumLength: 4096
                            placeholderText: qsTr("Confirm password")
                            Accessible.name: qsTr("Confirm keyring password")
                            onActiveFocusChanged: {
                                if (activeFocus) scroll.contentItem.contentY = Math.max(0, y + height - scroll.availableHeight)
                            }
                            onAccepted: prompt.approve(password.text, text, oldPassword.text)
                        }
                        RequestText { text: prompt.status; visible: text.length > 0 }
                    }
                }
                RowLayout {
                    id: footer
                    Layout.alignment: Qt.AlignRight
                    Tk.Button {
                        id: denyButton
                        objectName: "keyringDenyButton"
                        text: qsTr("Deny request")
                        onClicked: prompt.cancel()
                    }
                    Tk.Button {
                        id: reviewButton
                        objectName: "keyringReviewButton"
                        visible: !prompt.reviewed
                        enabled: prompt.requesterKnown
                        text: prompt.requesterKnown && !prompt.executableKnown ? qsTr("Continue without executable identity") : qsTr("Continue")
                        onClicked: { if (prompt.review()) root.focusInitialField() }
                    }
                    Tk.Button {
                        id: approveButton
                        objectName: "keyringApproveButton"
                        visible: prompt.reviewed
                        text: prompt.confirming ? qsTr("Delete item") : prompt.changing ? qsTr("Change password") : prompt.revealing ? qsTr("Reveal secret") : prompt.creation ? qsTr("Create keyring") : qsTr("Unlock keyring")
                        enabled: prompt.reviewed && (prompt.confirming || password.length > 0 && (!(prompt.creation || prompt.changing) || confirmation.length > 0) && (!prompt.changing || oldPassword.length > 0))
                        onClicked: prompt.approve(password.text, confirmation.text, oldPassword.text)
                    }
                }
            }
        }
    }
    Connections {
        target: prompt
        function onClearFields() { oldPassword.clear(); password.clear(); confirmation.clear(); root.focusInitialField() }
    }
}
