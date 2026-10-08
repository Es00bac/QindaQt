// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root
    property var configuration: null
    spacing: Tokens.space["2"]
    function clearEntry() { apiKey.clear() }
    onConfigurationChanged: clearEntry()
    Component.onDestruction: clearEntry()
    Connections {
        target: root.configuration
        function onClearEntry() { root.clearEntry() }
    }
    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("ElevenLabs credentials")
        description: qsTr("Gabbee saves the key in secure system storage. Reload uses an existing saved key, including after unlocking the keyring.")
    }
    Label {
        objectName: "voiceEffectiveProvider"
        Layout.fillWidth: true
        text: root.configuration ? root.configuration.effectiveText : qsTr("Effective provider is unconfirmed.")
        wrapMode: Text.Wrap
    }
    T.TextField {
        id: apiKey
        objectName: "voiceApiKeyEntry"
        Layout.fillWidth: true
        echoMode: TextInput.Password
        maximumLength: 512
        placeholderText: qsTr("ElevenLabs API key")
        enabled: root.configuration && root.configuration.available
        Accessible.name: qsTr("ElevenLabs API key")
        inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
    }
    RowLayout {
        Layout.fillWidth: true
        Button {
            objectName: "voiceSaveApiKey"
            text: qsTr("Save key securely")
            available: root.configuration && root.configuration.available && apiKey.text.length > 0
            busy: root.configuration && root.configuration.busy
            onClicked: {
                const submitted = apiKey.text
                root.clearEntry()
                if (root.configuration) root.configuration.save(submitted)
            }
        }
        Button {
            objectName: "voiceReloadApiKey"
            text: qsTr("Reload saved key")
            emphasized: false
            available: root.configuration && root.configuration.available
            onClicked: {
                root.clearEntry()
                if (root.configuration) root.configuration.reload()
            }
        }
    }
    Label {
        objectName: "voiceCredentialStatus"
        Layout.fillWidth: true
        text: root.configuration ? root.configuration.statusText
             : qsTr("This provider does not support credential settings.")
        wrapMode: Text.Wrap
        Accessible.name: text
    }
}
