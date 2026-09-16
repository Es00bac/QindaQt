// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One output bus (ADR-0173), condensed to a narrow card beside the input
// strips: name and target on top, meter and fader in the middle, transport
// and mode pads at the foot. Physical buses drive real devices; virtual buses
// are sinks other applications record from, which is how a streamer captures
// a submix.
Rectangle {
    id: root

    required property var model
    required property var bus
    required property bool enabledControls

    property bool rackOpen: false
    signal rackToggled()

    width: 120
    implicitHeight: busColumn.implicitHeight + Tokens.space["2"] * 2
    radius: Tokens.radius.s
    color: Tokens.bg.raised
    border.width: Tokens.space["1"] / 2
    border.color: Tokens.outline.divider
    objectName: "consoleBus_" + bus.id

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        anchors.rightMargin: 4
        anchors.leftMargin: 4
        height: 2
        radius: 1
        color: root.bus.virtual ? Tokens.status.warning.background : Tokens.accent.default
        opacity: 0.7
    }

    ColumnLayout {
        id: busColumn

        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: Tokens.space["1"]

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["1"]

            Label {
                Layout.fillWidth: true
                text: root.bus.label
                font.weight: Font.DemiBold
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                opacity: root.bus.bound ? 1.0 : 0.55
                Accessible.name: root.bus.bound
                    ? qsTr("Bus %1").arg(text)
                    : qsTr("Bus %1, no device connected").arg(text)
            }

            Label {
                text: root.bus.virtual ? qsTr("virtual") : qsTr("physical")
                font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })
                opacity: 0.7
                Accessible.ignored: true
            }
        }

        // The recorder (ADR-0184): one bus at a time, to a FLAC file. The pad
        // is a toggle on the published state, so two surfaces agree on which
        // bus is recording.
        AudioConsolePad {
            objectName: "consoleBusRecord_" + root.bus.id
            Layout.fillWidth: true
            readonly property var recording: root.model.consoleRecording ?? ({})
            readonly property bool thisBus: recording.active === true && recording.busId === root.bus.id
            text: thisBus ? qsTr("Stop") : qsTr("Record")
            checkable: true
            checked: thisBus
            destructive: true
            lampColor: Tokens.danger.default
            available: root.enabledControls && (recording.active !== true || thisBus)
            onClicked: checked ? root.model.startRecording(root.bus.id, "flac")
                               : root.model.stopRecording()
            Accessible.name: thisBus ? qsTr("Stop recording bus %1").arg(root.bus.label)
                                     : qsTr("Record bus %1").arg(root.bus.label)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["1"]

            AudioConsoleMeter {
                objectName: "consoleBusMeter_" + root.bus.id
                Layout.preferredWidth: 12
                Layout.fillHeight: true
                reading: root.model.consoleLevels[root.bus.id]
            }

            AudioConsoleFader {
                objectName: "consoleBusFader_" + root.bus.id
                Layout.preferredWidth: 40
                Layout.fillHeight: true
                model: root.model
                faderPosition: root.bus.faderPosition
                enabledControl: root.enabledControls
                accessibleName: qsTr("Bus %1 level").arg(root.bus.label)
                onMoved: position => root.model.setBusFader(root.bus.id, position)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                AudioConsolePad {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    objectName: "consoleBusRackToggle_" + root.bus.id
                    text: qsTr("Rack")
                    checkable: true
                    checked: root.rackOpen
                    visible: !root.bus.virtual
                    available: root.enabledControls
                    onClicked: root.rackToggled()
                    Accessible.name: qsTr("Rack for bus %1").arg(root.bus.label)
                }
                AudioConsolePad {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    objectName: "consoleBusMono_" + root.bus.id
                    text: qsTr("mono")
                    checkable: true
                    checked: root.bus.mono
                    available: root.enabledControls
                    onToggled: root.model.setBusMono(root.bus.id, checked)
                    Accessible.name: qsTr("Mono bus %1").arg(root.bus.label)
                }
                AudioConsolePad {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    objectName: "consoleBusMute_" + root.bus.id
                    text: qsTr("Mute")
                    checkable: true
                    checked: root.bus.muted
                    destructive: true
                    lampColor: Tokens.danger.default
                    available: root.enabledControls
                    onToggled: root.model.setBusMuted(root.bus.id, checked)
                    Accessible.name: qsTr("Mute bus %1").arg(root.bus.label)
                }
            }
        }

        // Which output device this bus feeds (ADR-0178), pinned the same way
        // the strip source is.
        AudioConsoleDevicePicker {
            objectName: "consoleBusTarget_" + root.bus.id
            Layout.fillWidth: true
            visible: !root.bus.virtual
            devices: root.model.outputDevices ?? []
            boundSerial: root.bus.targetSerial ?? 0
            pinned: root.bus.pinned ?? false
            enabled: root.enabledControls
            accessibleName: qsTr("Output for bus %1").arg(root.bus.label)
            onPicked: serial => root.model.setBusTarget(root.bus.id, serial)
        }
    }
}
