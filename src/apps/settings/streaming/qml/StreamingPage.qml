// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Settings → Streaming: what OBS is doing, the four things the desktop can
// ask it to do, and the setup that makes those work on a fresh machine.
T.Page {
    id: root

    required property var streamingSettings
    signal closeRequested()

    readonly property Item firstFocusTarget: setupButton.visible
                                             ? setupButton : connectButton

    Component.onCompleted: root.streamingSettings.refresh()

    title: qsTr("Streaming")
    background: Rectangle { color: Tokens.bg.base }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "streamingPageHeading"
            Layout.fillWidth: true
            text: qsTr("Streaming")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
            Accessible.description: qsTr("Record, stream and share a camera through OBS.")
        }

        Flickable {
            id: viewport
            objectName: "streamingFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: form.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            T.ScrollBar.vertical: T.ScrollBar {
                policy: T.ScrollBar.AsNeeded
                activeFocusOnTab: false
                Accessible.name: qsTr("Streaming settings scroll position")
            }

            ColumnLayout {
                id: form
                width: viewport.width
                spacing: Tokens.space["2"]

                Label {
                    objectName: "streamingStatusText"
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.streamingSettings.statusText
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }

                SectionHeader {
                    Layout.fillWidth: true
                    title: qsTr("OBS")
                    description: qsTr("QindaQt talks to OBS over its own control port on this machine")
                }

                Label {
                    objectName: "streamingConnectionState"
                    Layout.fillWidth: true
                    muted: true
                    text: root.streamingSettings.connectionDescription
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]

                    Button {
                        id: connectButton
                        objectName: "streamingConnectButton"
                        text: root.streamingSettings.connected
                              ? qsTr("Disconnect") : qsTr("Connect")
                        accessibleDescription: qsTr("Open or close QindaQt's connection to OBS")
                        onClicked: root.streamingSettings.connected
                                   ? root.streamingSettings.disconnectFromObs()
                                   : root.streamingSettings.connectToObs()
                    }

                    Button {
                        id: setupButton
                        objectName: "streamingSetupButton"
                        visible: !root.streamingSettings.defaultsInstalled
                        text: qsTr("Set up OBS")
                        accessibleDescription: qsTr("Write the QindaQt profile, scene collection and control-port settings into OBS")
                        onClicked: root.streamingSettings.installDefaults()
                    }

                    Button {
                        objectName: "streamingRepairButton"
                        visible: root.streamingSettings.defaultsInstalled
                        text: qsTr("Repair OBS setup")
                        emphasized: false
                        accessibleDescription: qsTr("Write QindaQt's OBS configuration again")
                        onClicked: root.streamingSettings.installDefaults()
                    }
                }

                Repeater {
                    model: root.streamingSettings.defaultsProblems
                    delegate: Label {
                        required property string modelData
                        objectName: "streamingSetupProblem"
                        Layout.fillWidth: true
                        muted: true
                        text: modelData
                    }
                }

                FormRow {
                    objectName: "streamingPortRow"
                    Layout.fillWidth: true
                    label: qsTr("Control port")
                    description: qsTr("The port OBS listens on. QindaQt only ever connects to this machine.")
                    editor: TextField {
                        id: portField
                        objectName: "streamingPortField"
                        width: 140
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 65535 }
                        accessibleName: qsTr("Control port")
                        accessibleDescription: qsTr("The port OBS listens on, 1 to 65535")
                        // AGENT-GUARD: commit on accept, not on every
                        // keystroke. Writing while the user is still typing
                        // would save "4" on the way to "4455" and reconnect
                        // against a port nothing is listening on.
                        text: String(root.streamingSettings.webSocketPort)
                        error: !acceptableInput
                        onEditingFinished: {
                            if (acceptableInput)
                                root.streamingSettings.setWebSocketPort(Number(text))
                            else
                                text = String(root.streamingSettings.webSocketPort)
                        }
                    }
                }

                FormRow {
                    objectName: "streamingAutoConnectRow"
                    Layout.fillWidth: true
                    label: qsTr("Connect automatically")
                    description: qsTr("Connect to OBS whenever this page opens")
                    editor: Switch {
                        objectName: "streamingAutoConnectSwitch"
                        checked: root.streamingSettings.autoConnect
                        accessibleDescription: qsTr("Whether QindaQt connects to OBS on its own")
                        onToggled: root.streamingSettings.setAutoConnect(checked)
                    }
                }

                FormRow {
                    objectName: "streamingStartAtLoginRow"
                    Layout.fillWidth: true
                    label: qsTr("Start OBS at login")
                    description: qsTr("Start OBS minimised when the session begins")
                    editor: Switch {
                        objectName: "streamingStartAtLoginSwitch"
                        checked: root.streamingSettings.startObsAtLogin
                        accessibleDescription: qsTr("Whether OBS starts with the session")
                        onToggled: root.streamingSettings.setStartObsAtLogin(checked)
                    }
                }

                StreamingOutputsSection {
                    objectName: "streamingOutputsSection"
                    Layout.fillWidth: true
                    streamingSettings: root.streamingSettings
                }

                StreamingBusMappingSection {
                    objectName: "streamingBusMappingSection"
                    Layout.fillWidth: true
                    streamingSettings: root.streamingSettings
                }
            }
        }
    }
}
