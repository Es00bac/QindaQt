// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One device-kind inventory list with default-device selection, volume, and
// mute intents.
ColumnLayout {
    id: root

    required property var audioSettings
    required property string sectionTitle
    required property string sectionDescription
    required property var deviceRows
    required property string emptyText
    required property string kindPrefix

    // The first projected row registers its entry control for the page's
    // focus entry and Tab cycle. The Repeater recreates every delegate on
    // each projection change, so registration refreshes with each model
    // reset; the destroying delegate clears it so a stale pointer is never
    // handed to forceActiveFocus.
    property Item firstActionTarget: null

    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    SectionHeader {
        Layout.fillWidth: true
        title: root.sectionTitle
        description: root.sectionDescription
    }

    Repeater {
        id: deviceRepeater
        model: root.deviceRows

        delegate: FormSurface {
            id: deviceRow
            required property var modelData
            required property int index
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1 %2, %3")
                .arg(deviceRow.modelData.kindText)
                .arg(deviceRow.modelData.displayName)
                .arg(deviceRow.modelData.stateText)

            Component.onCompleted: {
                if (deviceRow.index === 0) {
                    root.firstActionTarget = deviceRow.modelData.isDefault
                            ? levelRow.entryControl : setDefaultButton
                }
            }
            Component.onDestruction: {
                if (root.firstActionTarget === setDefaultButton
                        || root.firstActionTarget === levelRow.entryControl) {
                    root.firstActionTarget = null
                }
            }

            contentItem: ColumnLayout {
                spacing: Tokens.space["2"]

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["2"]

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Tokens.space["1"]

                        Label {
                            Layout.fillWidth: true
                            text: deviceRow.modelData.displayName
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: deviceRow.modelData.stateText
                            muted: true
                        }
                    }

                    Label {
                        visible: deviceRow.modelData.isDefault
                        text: qsTr("Default")
                        muted: true
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }

                    Button {
                        id: setDefaultButton
                        objectName: root.kindPrefix + "Default_"
                                    + deviceRow.modelData.serial
                        visible: !deviceRow.modelData.isDefault
                        available: deviceRow.modelData.setDefaultAvailable
                        busy: root.audioSettings.busy
                        emphasized: false
                        text: qsTr("Set default")
                        accessibleDescription: qsTr("Make %1 the default %2")
                            .arg(deviceRow.modelData.displayName)
                            .arg(deviceRow.modelData.kindText)
                        onClicked: root.audioSettings.setDefaultDevice(
                                       deviceRow.modelData.serial)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["3"]

                    AudioLevelRow {
                        id: levelRow
                        Layout.fillWidth: true
                        targetRow: deviceRow.modelData
                        kindPrefix: root.kindPrefix
                        targetName: deviceRow.modelData.displayName
                        commit: level => root.audioSettings.setDeviceVolume(
                                     deviceRow.modelData.serial, level)
                    }

                    Switch {
                        objectName: root.kindPrefix + "Mute_"
                                    + deviceRow.modelData.serial
                        text: qsTr("Mute")
                        checked: deviceRow.modelData.muted
                        enabled: deviceRow.modelData.muteAvailable
                        accessibleDescription: qsTr("Mute %1")
                            .arg(deviceRow.modelData.displayName)
                        onToggled: root.audioSettings.setDeviceMuted(
                                       deviceRow.modelData.serial, checked)
                    }
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: deviceRepeater.count === 0
        text: root.emptyText
        muted: true
    }
}
