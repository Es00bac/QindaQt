// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
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
        title: qsTr("Radios")
        description: qsTr("Turn Wi-Fi and mobile broadband on or off")
    }

    Repeater {
        model: root.networkSettings.radios

        delegate: FormSurface {
            id: radioRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]

            contentItem: ColumnLayout {
                spacing: Tokens.space["1"]

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["3"]

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        Label {
                            Layout.fillWidth: true
                            text: radioRow.modelData.name
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: radioRow.modelData.statusText
                            muted: true
                        }
                    }

                    Switch {
                        id: radioSwitch
                        objectName: radioRow.modelData.kind === 0
                                    ? "networkRadioWifi" : "networkRadioMobile"
                        text: ""
                        enabled: radioRow.modelData.controlAvailable
                        checked: radioRow.modelData.softwareEnabled
                        Accessible.name: qsTr("%1 radio").arg(radioRow.modelData.name)
                        accessibleDescription: radioRow.modelData.detailText.length > 0
                            ? radioRow.modelData.detailText
                            : radioRow.modelData.statusText
                        onToggled: {
                            root.networkSettings.setRadio(radioRow.modelData.kind, checked)
                            // AGENT-GUARD: T.Switch changes checked locally on keyboard
                            // activation. Rebind it to the accepted snapshot even when
                            // admission or the later service result refuses the choice.
                            Qt.callLater(function() {
                                radioSwitch.checked = Qt.binding(function() {
                                    return radioRow.modelData.softwareEnabled
                                })
                            })
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: radioRow.modelData.detailText
                    wrapMode: Text.Wrap
                    muted: !radioRow.modelData.pending
                    Accessible.name: text
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: root.networkSettings.radios.length === 0
        text: qsTr("No radio inventory is available.")
        muted: true
    }
}
