// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QtQuick.Window
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

Window {
    id: root

    required property var requestId
    required property string connectionName
    required property var fieldSpecs
    required property var presenter
    property bool completing: false

    objectName: "networkSecretPrompt"
    title: qsTr("Network credentials")
    width: 480
    height: Math.min(640, content.implicitHeight + Tokens.space["6"] * 2)
    minimumWidth: 360
    minimumHeight: 300
    modality: Qt.ApplicationModal
    visible: true
    color: Tokens.bg.base

    function wipeEditors() {
        for (let index = 0; index < fieldRepeater.count; ++index) {
            const item = fieldRepeater.itemAt(index)
            if (item !== null)
                item.editor.text = ""
        }
    }

    function wipeAndClose() {
        completing = true
        wipeEditors()
        close()
    }

    function submitPrompt() {
        const values = {}
        for (let index = 0; index < fieldRepeater.count; ++index) {
            const item = fieldRepeater.itemAt(index)
            values[item.fieldKey] = item.editor.text
        }
        completing = true
        wipeEditors()
        presenter.submit(requestId, values, rememberCheck.checked)
        close()
    }

    function cancelPrompt() {
        completing = true
        wipeEditors()
        presenter.cancelByUser(requestId)
        close()
    }

    onClosing: close => {
        if (!completing) {
            close.accepted = false
            cancelPrompt()
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        context: Qt.WindowShortcut
        onActivated: root.cancelPrompt()
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["4"]
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Credentials for %1").arg(root.connectionName)

        Label {
            Layout.fillWidth: true
            text: qsTr("Connect to %1").arg(root.connectionName)
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("NetworkManager requested the following fields. QindaQt keeps them only until this reply is sent.")
            wrapMode: Text.Wrap
            muted: true
            Accessible.name: text
        }

        FormSurface {
            Layout.fillWidth: true

            ColumnLayout {
                width: parent.width
                spacing: Tokens.space["3"]

                Repeater {
                    id: fieldRepeater
                    model: root.fieldSpecs

                    delegate: ColumnLayout {
                        id: fieldDelegate
                        required property var modelData
                        readonly property alias editor: fieldEditor
                        readonly property string fieldKey: modelData.key
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        Label {
                            text: fieldDelegate.modelData.label
                            Accessible.name: text
                        }

                        TextField {
                            id: fieldEditor
                            objectName: "networkSecretField-" + fieldDelegate.modelData.key
                            Layout.fillWidth: true
                            maximumLength: fieldDelegate.modelData.maximumLength
                            echoMode: fieldDelegate.modelData.concealed && !showCheck.checked
                                      ? TextInput.Password : TextInput.Normal
                            accessibleName: fieldDelegate.modelData.label
                            Accessible.description: qsTr("Required network credential")
                            onAccepted: root.submitPrompt()
                        }
                    }
                }

                CheckBox {
                    id: showCheck
                    objectName: "networkSecretShow"
                    text: qsTr("Show entered credentials")
                    Accessible.name: text
                    Accessible.description: checked
                                            ? qsTr("Entered credentials are visible")
                                            : qsTr("Entered credentials are concealed")
                }

                CheckBox {
                    id: rememberCheck
                    objectName: "networkSecretRemember"
                    text: qsTr("Remember using NetworkManager storage")
                    Accessible.name: text
                    Accessible.description: checked
                                            ? qsTr("NetworkManager may persist this credential")
                                            : qsTr("This credential is not saved")
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Item { Layout.fillWidth: true }

            Button {
                objectName: "networkSecretCancel"
                text: qsTr("Cancel")
                emphasized: false
                accessibleDescription: qsTr("Cancel this NetworkManager credential request")
                onClicked: root.cancelPrompt()
            }

            Button {
                objectName: "networkSecretSubmit"
                text: qsTr("Connect")
                accessibleDescription: qsTr("Send these credentials only to NetworkManager")
                onClicked: root.submitPrompt()
            }
        }
    }
}
