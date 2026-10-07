// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root
    required property var networkSettings
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Join a hidden Wi-Fi network")
        description: qsTr("Enter its exact name and security. A separate password prompt appears if needed.")
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Hidden network names use up to 32 UTF-8 bytes. Spaces are preserved.")
        textFormat: Text.PlainText
        muted: true
    }
    T.ComboBox {
        id: device
        objectName: "networkHiddenDevice"
        Layout.fillWidth: true
        model: root.networkSettings.hiddenNetworkDevices
        textRole: "interfaceName"
        valueRole: "interfaceName"
        Accessible.name: qsTr("Wi-Fi device")
    }
    T.TextField {
        id: ssid
        objectName: "networkHiddenSsid"
        Layout.fillWidth: true
        maximumLength: 32
        placeholderText: qsTr("Exact network name")
        Accessible.name: qsTr("Hidden network name")
        selectByMouse: true
        // AGENT-GUARD: Literal text is metadata; no trimming, rich-text label,
        // password field, or local credential storage belongs to this form.
    }
    T.ComboBox {
        id: security
        objectName: "networkHiddenSecurity"
        Layout.fillWidth: true
        model: [
            {name: qsTr("WPA2 Personal"), value: 2},
            {name: qsTr("WPA3 Personal"), value: 4}
        ]
        textRole: "name"
        valueRole: "value"
        Accessible.name: qsTr("Hidden network security")
    }
    Button {
        objectName: "networkHiddenJoin"
        text: qsTr("Join hidden network")
        busy: root.networkSettings.busy
        available: {
            // Tie the invokable verdict to published admission/lineage changes.
            const ready = root.networkSettings.ready
            const busy = root.networkSettings.busy
            const agent = root.networkSettings.secretAgentRegistered
            const revision = root.networkSettings.serviceRevision
            return ready && !busy && agent && revision > 0
                    && root.networkSettings.hiddenJoinAvailable(
                        device.currentValue === undefined ? "" : device.currentValue,
                        ssid.text, security.currentValue)
        }
        accessibleDescription: qsTr("Create a profile and ask NetworkManager to connect. A password is entered only in the separate prompt.")
        onClicked: root.networkSettings.connectHiddenNetwork(
                       device.currentValue, ssid.text, security.currentValue)
    }
}
