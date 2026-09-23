// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk

// ADR-0246: explicit two-end manual setup. Saving a receiver grants one
// source/port/output permission; only its separate Enable action opens UDP.
ColumnLayout {
    id: root
    required property var audioSettings
    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    readonly property Item firstActionTarget: connectionCard.firstActionTarget
    readonly property Item lastActionTarget: receiveSave.enabled ? receiveSave : null
    readonly property var buses: audioSettings.peerBuses ?? []
    readonly property var outputs: audioSettings.peerOutputs ?? []
    readonly property var definitions: audioSettings.consoleVban ?? []

    function busIndex(id) {
        for (let i = 0; i < buses.length; ++i)
            if (buses[i].id === id) return i
        return -1
    }
    function outputIndex(nodeName) {
        for (let i = 0; i < outputs.length; ++i)
            if (outputs[i].nodeName === nodeName) return i
        return -1
    }
    function editDefinition(row) {
        if (row.outgoing) {
            sendName.text = row.name
            sendHost.text = row.host
            sendPort.text = String(row.port)
            sendBus.currentIndex = busIndex(row.busId)
            sendName.forceActiveFocus(Qt.OtherFocusReason)
        } else {
            receiveName.text = row.name
            receiveSource.text = row.host
            receivePort.text = String(row.port)
            receiveOutput.currentIndex = outputIndex(row.outputNodeName)
            receiveName.forceActiveFocus(Qt.OtherFocusReason)
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Other computers")
        description: qsTr("Stereo audio with another computer on your local network")
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("Use a connection code to copy the sender details to the other computer, or use the manual forms below. Each computer needs its own send and receive setup.")
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("Network audio is sent openly, without encryption or a check of the other computer's identity. Only turn on Receive on a trusted network. 'Local route active' means this computer's audio route is connected; it does not prove sound played on the other computer.")
        muted: true
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
    Label {
        objectName: "audioPeerUnavailable"
        Layout.fillWidth: true
        visible: !root.audioSettings.canManagePeerStreams
        text: qsTr("Audio1 cannot edit network streams right now.")
        wrapMode: Text.WordWrap
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    AudioPeerConnectionCard {
        id: connectionCard
        objectName: "audioPeerConnectionCard"
        audioSettings: root.audioSettings
        definitions: root.definitions
        outputs: root.outputs
    }

    Repeater {
        model: root.definitions.length
        delegate: FormSurface {
            id: row
            required property int index
            readonly property var definition: root.definitions[index] ?? ({})
            Layout.fillWidth: true
            padding: Tokens.space["2"]
            contentItem: ColumnLayout {
                spacing: Tokens.space["1"]
                Label {
                    Layout.fillWidth: true
                    text: row.definition.outgoing
                          ? qsTr("Send %1 · %2:%3 · %4")
                              .arg(row.definition.name).arg(row.definition.host)
                              .arg(row.definition.port).arg(row.definition.busId)
                          : qsTr("Receive %1 · only %2:%3 · %4")
                              .arg(row.definition.name).arg(row.definition.host)
                              .arg(row.definition.port).arg(row.definition.outputNodeName)
                    wrapMode: Text.WrapAnywhere
                    font.weight: Font.DemiBold
                    Accessible.name: text
                }
                Label {
                    objectName: "audioPeerState_" + row.definition.name
                    Layout.fillWidth: true
                    text: !row.definition.enabled ? qsTr("Disabled")
                        : row.definition.active
                          ? qsTr("Local route active · remote audibility unconfirmed")
                          : qsTr("Enabled · waiting for local device or graph")
                    muted: true
                    Accessible.name: text
                }
                RowLayout {
                    Layout.fillWidth: true
                    Tk.Button {
                        objectName: "audioPeerEnable_" + row.definition.name
                        small: true
                        text: row.definition.enabled ? qsTr("Disable") : qsTr("Enable")
                        available: root.audioSettings.canManagePeerStreams
                        onClicked: root.audioSettings.setVbanEnabled(
                                       row.definition.name, !row.definition.enabled)
                        Accessible.name: text + " " + row.definition.name
                    }
                    Tk.Button {
                        objectName: "audioPeerEdit_" + row.definition.name
                        small: true
                        text: qsTr("Edit")
                        available: root.audioSettings.canManagePeerStreams
                        onClicked: root.editDefinition(row.definition)
                        Accessible.name: qsTr("Edit %1").arg(row.definition.name)
                    }
                    Tk.Button {
                        objectName: "audioPeerRemove_" + row.definition.name
                        small: true
                        text: qsTr("Remove")
                        available: root.audioSettings.canManagePeerStreams
                        onClicked: root.audioSettings.removePeer(row.definition.name)
                        Accessible.name: qsTr("Remove %1").arg(row.definition.name)
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Send to a computer")
        description: qsTr("Your mixer bus is sent as 48 kHz stereo to the receiver's address.")
    }
    Tk.TextField {
        id: sendName
        objectName: "audioPeerSendName"
        Layout.fillWidth: true
        placeholderText: qsTr("Stream name (same on both computers)")
        tooltip: qsTr("Outgoing VBAN stream name, up to 16 ASCII characters")
        enabled: root.audioSettings.canManagePeerStreams
    }
    Tk.ComboBox {
        id: sendBus
        objectName: "audioPeerSendBus"
        Layout.fillWidth: true
        model: root.buses
        textRole: "label"
        placeholderText: qsTr("Choose a mixer bus")
        tooltip: qsTr("Local mixer bus to send")
        enabled: root.audioSettings.canManagePeerStreams && root.buses.length > 0
        currentIndex: root.buses.length > 0 ? 0 : -1
    }
    Tk.TextField {
        id: sendHost
        objectName: "audioPeerSendHost"
        Layout.fillWidth: true
        placeholderText: qsTr("Receiver IPv4 address or host name")
        tooltip: qsTr("The receiving computer's local-network address")
        enabled: root.audioSettings.canManagePeerStreams
    }
    Tk.TextField {
        id: sendPort
        objectName: "audioPeerSendPort"
        Layout.fillWidth: true
        text: "6980"
        inputMethodHints: Qt.ImhDigitsOnly
        validator: IntValidator { bottom: 1; top: 65535 }
        tooltip: qsTr("Receiver UDP port, 1 to 65535")
        enabled: root.audioSettings.canManagePeerStreams
    }
    Tk.Button {
        id: sendSave
        objectName: "audioPeerSendSave"
        Layout.alignment: Qt.AlignLeft
        text: qsTr("Save send stream")
        available: root.audioSettings.canManagePeerStreams
                   && sendName.text.trim().length > 0
                   && sendHost.text.trim().length > 0
                   && sendBus.currentIndex >= 0
                   && sendPort.acceptableInput
        onClicked: root.audioSettings.saveOutgoingPeer(
                       sendName.text, root.buses[sendBus.currentIndex].id,
                       sendHost.text, Number(sendPort.text))
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Receive from a computer")
        description: qsTr("Allow one exact sender IPv4 address and select this computer's speakers.")
    }
    Tk.TextField {
        id: receiveName
        objectName: "audioPeerReceiveName"
        Layout.fillWidth: true
        placeholderText: qsTr("Stream name (same on both computers)")
        tooltip: qsTr("Incoming VBAN stream name, up to 16 ASCII characters")
        enabled: root.audioSettings.canManagePeerStreams
    }
    Tk.TextField {
        id: receiveSource
        objectName: "audioPeerReceiveSource"
        Layout.fillWidth: true
        placeholderText: qsTr("Allowed sender IPv4 address")
        tooltip: qsTr("Only datagrams from this exact IPv4 source can play")
        enabled: root.audioSettings.canManagePeerStreams
    }
    Tk.ComboBox {
        id: receiveOutput
        objectName: "audioPeerReceiveOutput"
        Layout.fillWidth: true
        model: root.outputs
        textRole: "label"
        placeholderText: qsTr("Choose this computer's speakers")
        tooltip: qsTr("Exact physical output device; no fallback to another speaker")
        enabled: root.audioSettings.canManagePeerStreams && root.outputs.length > 0
        currentIndex: root.outputs.length > 0 ? 0 : -1
    }
    Tk.TextField {
        id: receivePort
        objectName: "audioPeerReceivePort"
        Layout.fillWidth: true
        text: "6980"
        inputMethodHints: Qt.ImhDigitsOnly
        validator: IntValidator { bottom: 1; top: 65535 }
        tooltip: qsTr("UDP port listened on this computer")
        enabled: root.audioSettings.canManagePeerStreams
    }
    Tk.Button {
        id: receiveSave
        objectName: "audioPeerReceiveSave"
        Layout.alignment: Qt.AlignLeft
        text: qsTr("Save receive permission")
        available: root.audioSettings.canManagePeerStreams
                   && receiveName.text.trim().length > 0
                   && receiveSource.text.trim().length > 0
                   && receiveOutput.currentIndex >= 0
                   && receivePort.acceptableInput
        onClicked: root.audioSettings.saveIncomingPeer(
                       receiveName.text, receiveSource.text,
                       root.outputs[receiveOutput.currentIndex].nodeName,
                       Number(receivePort.text))
    }
}
