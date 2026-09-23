// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk

// ADR-0252: a code transfers configuration, never trust or Audio1 authority.
FormSurface {
    id: root
    required property var audioSettings
    required property var definitions
    required property var outputs
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    padding: Tokens.space["2"]

    property var shared: ({})
    property var reviewed: ({})
    property string notice: ""
    readonly property var senders: root.definitions.filter(row => row.outgoing)
    property var addresses: []
    Component.onCompleted: refreshAddresses()

    function refreshAddresses() {
        addresses = audioSettings.localPeerAddresses()
    }
    readonly property Item firstActionTarget: senderChoice.enabled ? senderChoice
                                              : senderAddress.enabled ? senderAddress : null
    readonly property Item lastActionTarget: importSave
    onSendersChanged: shared = ({})

    function reasonText(reason) {
        if (reason === "foreign-code")
            return qsTr("This code is from a different or newer connection format.")
        if (reason === "duplicate-peer")
            return qsTr("This receive stream is already saved.")
        if (reason === "name-conflict")
            return qsTr("A stream with this name already exists. Remove or edit it first.")
        if (reason === "port-conflict")
            return qsTr("Another receive stream already uses this port.")
        if (reason === "unavailable")
            return qsTr("Wait for the audio service, then try again.")
        return qsTr("Check the code and try again.")
    }

    contentItem: ColumnLayout {
        spacing: Tokens.space["2"]

        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Connect with a code")
            description: qsTr("Copy the receiving computer's address first, then share the sender's saved stream.")
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("On the sending computer")
            font.weight: Font.DemiBold
        }
        Tk.ComboBox {
            id: senderChoice
            objectName: "audioPeerCodeSender"
            Layout.fillWidth: true
            model: root.senders
            textRole: "name"
            placeholderText: qsTr("Choose a saved send stream")
            enabled: root.audioSettings.canManagePeerStreams && root.senders.length > 0
            currentIndex: root.senders.length > 0 ? 0 : -1
            onActivated: root.shared = ({})
            Accessible.name: qsTr("Saved send stream to share")
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("This computer's address")
            font.weight: Font.DemiBold
        }
        Tk.ComboBox {
            id: addressChoice
            objectName: "audioPeerCodeAddressChoice"
            Layout.fillWidth: true
            model: root.addresses
            textRole: "label"
            placeholderText: qsTr("Choose this computer's address")
            currentIndex: root.addresses.length > 0 ? 0 : -1
            enabled: root.addresses.length > 0
            onActivated: index => senderAddress.text = root.addresses[index].address
            Accessible.name: qsTr("This computer's local network address")
        }
        Tk.Button {
            objectName: "audioPeerCodeRefreshAddresses"
            text: qsTr("Refresh addresses")
            onClicked: root.refreshAddresses()
        }
        Tk.TextField {
            id: senderAddress
            objectName: "audioPeerCodeSenderAddress"
            Layout.fillWidth: true
            text: root.addresses.length > 0 ? root.addresses[0].address : ""
            placeholderText: qsTr("This computer's IPv4 address")
            tooltip: qsTr("You can enter a different local or VPN address.")
            enabled: root.audioSettings.canManagePeerStreams
            onTextChanged: root.shared = ({})
            Accessible.name: qsTr("Address other computer can reach")
        }
        Tk.Button {
            id: copyAddress
            objectName: "audioPeerCodeCopyAddress"
            text: qsTr("Copy this address")
            available: senderAddress.text.length > 0
            onClicked: {
                senderAddress.selectAll()
                senderAddress.copy()
            }
        }
        Tk.Button {
            id: generate
            objectName: "audioPeerCodeGenerate"
            text: qsTr("Make connection code")
            available: root.audioSettings.canManagePeerStreams
                       && senderChoice.currentIndex >= 0 && senderAddress.text.length > 0
            onClicked: {
                root.shared = root.audioSettings.sharePeerCode(
                    root.senders[senderChoice.currentIndex].name, senderAddress.text)
                root.notice = root.shared.valid ? "" : root.reasonText(root.shared.reason)
            }
        }
        Tk.TextField {
            id: shareCode
            objectName: "audioPeerCodeShare"
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            visible: root.shared.valid === true
            readOnly: true
            selectByMouse: true
            text: root.shared.code ?? ""
            Accessible.name: qsTr("Connection code to copy to the other computer")
        }
        Tk.Button {
            id: copy
            objectName: "audioPeerCodeCopy"
            text: qsTr("Copy code")
            visible: root.shared.valid === true
            available: root.shared.valid === true
            onClicked: {
                shareCode.selectAll()
                shareCode.copy()
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("On the receiving computer")
            font.weight: Font.DemiBold
        }
        Tk.TextField {
            id: pastedCode
            objectName: "audioPeerCodePaste"
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            placeholderText: qsTr("Paste connection code")
            tooltip: qsTr("The code includes the sender's name, address and port.")
            enabled: root.audioSettings.canManagePeerStreams
            onTextChanged: {
                root.reviewed = ({})
                root.notice = ""
                trust.checked = false
            }
            Accessible.name: qsTr("Connection code from the sending computer")
        }
        Tk.Button {
            id: review
            objectName: "audioPeerCodeReview"
            text: qsTr("Review connection")
            available: root.audioSettings.canManagePeerStreams
                       && pastedCode.text.trim().length > 0
            onClicked: {
                root.reviewed = root.audioSettings.reviewPeerCode(pastedCode.text)
                root.notice = root.reviewed.valid ? "" : root.reasonText(root.reviewed.reason)
            }
        }
        Label {
            objectName: "audioPeerCodeReviewSummary"
            Layout.fillWidth: true
            visible: root.reviewed.valid === true
            wrapMode: Text.WordWrap
            text: qsTr("Receive %1 from %2 on port %3")
                    .arg(root.reviewed.name ?? "")
                    .arg(root.reviewed.sourceIpv4 ?? "")
                    .arg(root.reviewed.port ?? "")
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        Tk.ComboBox {
            id: outputChoice
            objectName: "audioPeerCodeOutput"
            Layout.fillWidth: true
            visible: root.reviewed.valid === true
            model: root.outputs
            textRole: "label"
            placeholderText: qsTr("Choose this computer's speakers")
            currentIndex: root.outputs.length > 0 ? 0 : -1
            enabled: root.audioSettings.canManagePeerStreams && root.outputs.length > 0
            Accessible.name: qsTr("Exact speakers for received audio")
        }
        Label {
            Layout.fillWidth: true
            visible: root.reviewed.valid === true
            wrapMode: Text.WordWrap
            text: qsTr("Audio travels openly over the local network, without encryption or identity checks. Use only on a trusted network. The code does not grant permission by itself.")
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        Tk.CheckBox {
            id: trust
            objectName: "audioPeerCodeTrust"
            Layout.fillWidth: true
            visible: root.reviewed.valid === true
            text: qsTr("I trust this network and sender")
        }
        Tk.Button {
            id: importSave
            objectName: "audioPeerCodeSave"
            text: qsTr("Save receive permission")
            visible: root.reviewed.valid === true
            available: root.audioSettings.canManagePeerStreams
                       && root.reviewed.valid === true
                       && outputChoice.currentIndex >= 0 && trust.checked
            onClicked: {
                if (root.audioSettings.saveImportedPeer(
                        pastedCode.text, root.outputs[outputChoice.currentIndex].nodeName)) {
                    root.notice = qsTr("Save requested. After the stream appears, select Enable to start receiving.")
                    trust.checked = false
                } else {
                    root.reviewed = root.audioSettings.reviewPeerCode(pastedCode.text)
                    root.notice = root.reasonText(root.reviewed.reason)
                }
            }
        }
        Label {
            objectName: "audioPeerCodeNotice"
            Layout.fillWidth: true
            visible: root.notice.length > 0
            wrapMode: Text.WordWrap
            text: root.notice
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
    }
}
