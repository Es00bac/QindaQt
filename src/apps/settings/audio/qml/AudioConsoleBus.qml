// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// One output bus (ADR-0173) as a desk card beside the input strips: name,
// the device this bus drives, the channel-mode lamps (the reference console
// puts bus mode on the bus face, not in a menu), the mono/mute lamps, then
// meter, fader with its dB scale, and the card's actions. Physical buses
// drive real devices; virtual buses are sinks other applications record
// from, which is how a streamer captures a submix.
//
// AGENT-CONTRACT: the six band heights below are shared VERBATIM with
// AudioConsoleStrip.qml, in the same order, and a band that does not apply to
// a card keeps its slot rather than collapsing. See the longer note in that
// file; qindaqt.settings-audio-page's consoleCardsShareOneGrid fails if they
// drift apart.
Rectangle {
    id: root

    required property var model
    required property var bus
    required property bool enabledControls

    property bool rackOpen: false
    signal rackToggled()

    readonly property int headerHeight: 18
    readonly property int assignmentHeight: 24
    readonly property int controlHeight: 62
    readonly property int routingHeight: 40
    readonly property int deskHeight: 150
    readonly property int actionHeight: 18

    // The bus channel modes, in the order the reference console lists them.
    // Kept beside the definition of the grid so the pads and the rack agree.
    readonly property var modeEntries: [
        { token: "normal", label: qsTr("Stereo"), name: qsTr("Stereo mode") },
        { token: "swap", label: qsTr("Swap"), name: qsTr("Swap left and right") },
        { token: "left", label: qsTr("Left"), name: qsTr("Left to both") },
        { token: "right", label: qsTr("Right"), name: qsTr("Right to both") }
    ]
    readonly property string currentMode: bus.processing?.mode ?? "normal"

    width: 140
    implicitHeight: busColumn.implicitHeight + Tk.Theme.space.sm * 2
    radius: Tk.Theme.radius.sm
    color: Tk.Theme.color.panelAlt
    border.width: Tk.Theme.size.border
    border.color: Tk.Theme.color.border
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
        color: root.bus.virtual ? Tk.Theme.color.warning : Tk.Theme.color.accent
        opacity: 0.7
    }

    Tk.Flex {
        id: busColumn

        anchors.fill: parent
        anchors.margins: Tk.Theme.space.sm
        direction: Tk.Flex.Column
        gap: 2

        // Band 1 — name.
        Item {
            implicitHeight: root.headerHeight
            Tk.Flex.shrink: 0

            Tk.Flex {
                anchors.fill: parent
                align: Tk.Flex.Center
                gap: Tk.Theme.space.xs

                Tk.Label {
                    Tk.Flex.grow: 1
                    text: root.bus.label
                    font.weight: Font.DemiBold
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                    opacity: root.bus.bound ? 1.0 : 0.55
                    Accessible.name: root.bus.bound
                        ? qsTr("Bus %1").arg(text)
                        : qsTr("Bus %1, no device connected").arg(text)
                }
                Tk.Caption {
                    text: root.bus.virtual ? qsTr("virtual") : qsTr("physical")
                    Accessible.ignored: true
                    Tk.Flex.shrink: 0
                }
            }
        }

        // Band 2 — assignment. Which output device this bus feeds (ADR-0178).
        // A virtual bus drives no device, so the slot is emptied rather than
        // removed.
        Item {
            implicitHeight: root.assignmentHeight
            Tk.Flex.shrink: 0

            AudioConsoleDevicePicker {
                objectName: "consoleBusTarget_" + root.bus.id
                anchors.fill: parent
                opacity: root.bus.virtual ? 0.0 : 1.0
                devices: root.model.outputDevices ?? []
                boundSerial: root.bus.targetSerial ?? 0
                pinned: root.bus.pinned ?? false
                enabled: root.enabledControls && !root.bus.virtual
                Accessible.ignored: root.bus.virtual
                accessibleName: qsTr("Output for bus %1").arg(root.bus.label)
                onPicked: serial => root.model.setBusTarget(root.bus.id, serial)
            }
        }

        // Band 3 — the channel mode, on the bus face where the reference
        // console puts it (ADR-0180). A virtual bus has no rack and no mode,
        // so the slot is emptied rather than removed.
        Item {
            implicitHeight: root.controlHeight
            Tk.Flex.shrink: 0

            Tk.Flex {
                anchors.centerIn: parent
                width: parent.width
                direction: Tk.Flex.Column
                gap: 2
                opacity: root.bus.virtual ? 0.0 : 1.0

                Repeater {
                    model: 2
                    delegate: Tk.Flex {
                        required property int index
                        readonly property var rowEntries:
                            root.modeEntries.slice(index * 2, index * 2 + 2)
                        gap: 2
                        Repeater {
                            model: rowEntries
                            AudioConsolePad {
                                id: modePad
                                required property var modelData
                                readonly property var entry: modelData
                                Tk.Flex.grow: 1
                                implicitHeight: 18
                                objectName: "consoleBusMode_" + root.bus.id + "_" + entry.token
                                text: entry.label
                                checkable: true
                                // The mode lamps read as one lit key, like the
                                // reference console's radio row: the
                                // projection owns the lit state (ADR-0191).
                                Binding on checked { value: root.currentMode === entry.token; when: !modePad.down }
                                available: root.enabledControls
                                enabled: !root.bus.virtual
                                Accessible.ignored: root.bus.virtual
                                Accessible.name: qsTr("%1 for bus %2")
                                    .arg(entry.name).arg(root.bus.label)
                                onClicked: root.model.setBusProcessing(
                                    root.bus.id, { mode: entry.token })
                            }
                        }
                    }
                }
            }
        }

        // Band 4 — mono and mute lamps, one row each, level with the strips'
        // routing rows.
        Item {
            implicitHeight: root.routingHeight
            Tk.Flex.shrink: 0

            Tk.Flex {
                anchors.fill: parent
                direction: Tk.Flex.Column
                gap: 2

                AudioConsolePad {
                    id: busMonoPad
                    objectName: "consoleBusMono_" + root.bus.id
                    implicitHeight: 18
                    text: qsTr("mono")
                    checkable: true
                    available: root.enabledControls
                    Binding on checked { value: root.bus.mono; when: !busMonoPad.down }
                    onToggled: root.model.setBusMono(root.bus.id, checked)
                    Accessible.name: qsTr("Mono bus %1").arg(root.bus.label)
                }
                AudioConsolePad {
                    id: busMutePad
                    objectName: "consoleBusMute_" + root.bus.id
                    implicitHeight: 18
                    text: qsTr("Mute")
                    checkable: true
                    destructive: true
                    lampColor: Tk.Theme.color.danger
                    lampTextColor: Tk.Theme.color.dangerContrast
                    available: root.enabledControls
                    Binding on checked { value: root.bus.muted; when: !busMutePad.down }
                    onToggled: root.model.setBusMuted(root.bus.id, checked)
                    Accessible.name: qsTr("Mute bus %1").arg(root.bus.label)
                }
            }
        }

        // Band 5 — the desk: meter, fader with its dB scale.
        Item {
            implicitHeight: root.deskHeight
            Tk.Flex.shrink: 0

            Tk.Flex {
                anchors.fill: parent
                gap: Tk.Theme.space.xs

                AudioConsoleMeter {
                    objectName: "consoleBusMeter_" + root.bus.id
                    implicitWidth: 14
                    implicitHeight: root.deskHeight
                    reading: root.model.consoleLevels[root.bus.id]
                    Tk.Flex.shrink: 0
                }

                AudioConsoleFader {
                    objectName: "consoleBusFader_" + root.bus.id
                    implicitHeight: root.deskHeight
                    Tk.Flex.grow: 1
                    model: root.model
                    faderPosition: root.bus.faderPosition
                    enabledControl: root.enabledControls
                    accessibleName: qsTr("Bus %1 level").arg(root.bus.label)
                    onMoved: position => root.model.setBusFader(root.bus.id, position)
                }
            }
        }

        // Band 6 — actions. The recorder (ADR-0184): one bus at a time, to a
        // FLAC file. The pad is a toggle on the published state, so two
        // surfaces agree on which bus is recording.
        Item {
            implicitHeight: root.actionHeight
            Tk.Flex.shrink: 0

            Tk.Flex {
                anchors.fill: parent
                gap: Tk.Theme.space.xs

                AudioConsolePad {
                    id: recordPad
                    objectName: "consoleBusRecord_" + root.bus.id
                    implicitHeight: root.actionHeight
                    Tk.Flex.grow: 1
                    readonly property var recording: root.model.consoleRecording ?? ({})
                    readonly property bool thisBus: recording.active === true && recording.busId === root.bus.id
                    text: thisBus ? qsTr("Stop") : qsTr("Record")
                    checkable: true
                    Binding on checked { value: recordPad.thisBus; when: !recordPad.down }
                    destructive: true
                    lampColor: Tk.Theme.color.danger
                    lampTextColor: Tk.Theme.color.dangerContrast
                    available: root.enabledControls && (recording.active !== true || thisBus)
                    onClicked: checked ? root.model.startRecording(root.bus.id, "flac")
                                       : root.model.stopRecording()
                    Accessible.name: thisBus ? qsTr("Stop recording bus %1").arg(root.bus.label)
                                             : qsTr("Record bus %1").arg(root.bus.label)
                }

                AudioConsolePad {
                    id: busRackPad
                    objectName: "consoleBusRackToggle_" + root.bus.id
                    implicitHeight: root.actionHeight
                    Tk.Flex.grow: 1
                    text: qsTr("Rack")
                    checkable: true
                    Binding on checked { value: root.rackOpen; when: !busRackPad.down }
                    // A virtual bus has no rack; the slot stays so the action
                    // band keeps its height beside the physical buses.
                    opacity: root.bus.virtual ? 0.0 : 1.0
                    enabled: !root.bus.virtual
                    available: root.enabledControls
                    Accessible.ignored: root.bus.virtual
                    onClicked: root.rackToggled()
                    Accessible.name: qsTr("Rack for bus %1").arg(root.bus.label)
                }
            }
        }
    }
}
