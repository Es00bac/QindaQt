// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// One input channel strip (ADR-0173) as a desk card, Voicemeeter Potato
// style: name, the device this strip follows, a control band (pan dial plus
// mono/solo/mute lamps), the routing bank (one lamp per bus, physical buses
// above virtual ones, exactly the A-row-over-B-row the reference console
// prints), then meter, fader with its dB scale, and the card's actions.
// Cards flow left-to-right and wrap (AudioConsoleSection), so the console
// uses whatever width the window offers and never scrolls horizontally.
//
// AGENT-CONTRACT: the six band heights below are shared VERBATIM with
// AudioConsoleBus.qml, in the same order, and a band that does not apply to
// a card keeps its slot rather than collapsing. Cards are read across a row
// like a real desk, so every meter, fader and lamp must sit at the same
// height as its neighbour's. qindaqt.settings-audio-console-page's
// consoleCardsShareOneGrid fails if they drift apart.
Rectangle {
    id: root

    required property var model
    required property var strip
    required property var buses
    required property bool soloActive
    required property bool enabledControls

    // Owned here rather than on the toggle so a model republish that rebuilds
    // the card keeps the rack open.
    property bool rackVisible: false
    signal rackToggled()

    readonly property int headerHeight: 18
    readonly property int assignmentHeight: 24
    readonly property int controlHeight: 62
    readonly property int routingHeight: 40
    readonly property int deskHeight: 150
    readonly property int actionHeight: 18

    // AGENT-GUARD: the card's width must arrive through implicitWidth.
    // QindaTK's Flex reads only implicit sizes and attached
    // constraints and IGNORES a child's own width (docs/layout.md),
    // so `width: 140` here crushed the card to zero and let every
    // band overflow. consoleCardsShareOneGrid asserts the width.
    implicitWidth: 140
    implicitHeight: stripColumn.implicitHeight + Tk.Theme.space.sm * 2
    radius: Tk.Theme.radius.sm
    color: Tk.Theme.color.panelAlt
    border.width: Tk.Theme.size.border
    border.color: Tk.Theme.color.border
    objectName: "consoleStrip_" + strip.id

    // Hardware inputs take the accent colour, virtual inputs the warning
    // colour - the stripe reads as "kind", not decoration, and the name band
    // also says it in words (no colour-only state).
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        anchors.rightMargin: 4
        anchors.leftMargin: 4
        height: 2
        radius: 1
        color: root.strip.virtual ? Tk.Theme.color.warning : Tk.Theme.color.accent
        opacity: 0.7
    }

    Tk.Flex {
        id: stripColumn

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
                    text: root.strip.label
                    font.weight: Font.DemiBold
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                    // A strip whose device is absent stays usable; it is dimmed
                    // rather than removed so the user's routing does not
                    // vanish with the device.
                    opacity: root.strip.bound ? 1.0 : 0.55
                    Accessible.name: root.strip.bound
                        ? text : qsTr("%1, no device connected").arg(text)
                }
            }
        }

        // Band 2 — assignment. Which microphone this strip follows (ADR-0178).
        // A virtual strip has nothing to assign, so the slot is emptied
        // rather than removed.
        Item {
            implicitHeight: root.assignmentHeight
            Tk.Flex.shrink: 0

            AudioConsoleDevicePicker {
                objectName: "consoleStripSource_" + root.strip.id
                anchors.fill: parent
                opacity: root.strip.virtual ? 0.0 : 1.0
                devices: root.model.inputDevices ?? []
                boundSerial: root.strip.sourceSerial ?? 0
                pinned: root.strip.pinned ?? false
                enabled: root.enabledControls && !root.strip.virtual
                Accessible.ignored: root.strip.virtual
                accessibleName: qsTr("Input for %1").arg(root.strip.label)
                onPicked: serial => root.model.setStripSource(root.strip.id, serial)
            }
        }

        // Band 3 — control: the pan dial beside the mono/solo/mute lamps.
        Item {
            implicitHeight: root.controlHeight
            Tk.Flex.shrink: 0

            Tk.Flex {
                anchors.fill: parent
                gap: Tk.Theme.space.xs

                AudioConsoleKnob {
                    id: panKnob
                    objectName: "consoleStripPan_" + root.strip.id
                    Tk.Flex.grow: 1
                    implicitHeight: root.controlHeight
                    label: qsTr("Pan %1").arg(root.strip.label)
                    showLabel: false
                    from: -1.0
                    to: 1.0
                    value: root.strip.pan ?? 0.0
                    defaultValue: 0.0
                    enabledControl: root.enabledControls
                    // Balance reads as L/C/R, not as a bare number: this is
                    // the same -1..+1 the model publishes (ADR-0177).
                    formatValue: v => Math.abs(v) < 0.025 ? qsTr("C")
                        : (v < 0 ? qsTr("L %1").arg(Math.round(-v * 100))
                                 : qsTr("R %1").arg(Math.round(v * 100)))
                    onCommitted: v => root.model.setStripPan(root.strip.id, v)
                }

                Tk.Flex {
                    direction: Tk.Flex.Column
                    gap: 2
                    implicitWidth: 44
                    Tk.Flex.shrink: 0

                    AudioConsolePad {
                        id: monoPad
                        objectName: "consoleStripMono_" + root.strip.id
                        implicitHeight: 18
                        text: qsTr("mono")
                        checkable: true
                        available: root.enabledControls
                        // AGENT-CONTRACT (ADR-0191): a held lamp owns its
                        // state; the projection rebinds the moment the gesture
                        // ends, so a refused change can never leave a lamp lit.
                        Binding on checked { value: root.strip.mono; when: !monoPad.down }
                        onToggled: root.model.setStripMono(root.strip.id, checked)
                        Accessible.name: qsTr("Mono %1").arg(root.strip.label)
                    }
                    AudioConsolePad {
                        id: soloPad
                        objectName: "consoleStripSolo_" + root.strip.id
                        implicitHeight: 18
                        text: qsTr("solo")
                        checkable: true
                        lampColor: Tk.Theme.color.warning
                        lampTextColor: Tk.Theme.color.canvas
                        available: root.enabledControls
                        Binding on checked { value: root.strip.soloed; when: !soloPad.down }
                        onToggled: root.model.setStripSoloed(root.strip.id, checked)
                        Accessible.name: qsTr("Solo %1").arg(root.strip.label)
                    }
                    AudioConsolePad {
                        id: mutePad
                        objectName: "consoleStripMute_" + root.strip.id
                        implicitHeight: 18
                        text: qsTr("Mute")
                        checkable: true
                        destructive: true
                        lampColor: Tk.Theme.color.danger
                        lampTextColor: Tk.Theme.color.dangerContrast
                        available: root.enabledControls
                        Binding on checked { value: root.strip.muted; when: !mutePad.down }
                        onToggled: root.model.setStripMuted(root.strip.id, checked)
                        Accessible.name: qsTr("Mute %1").arg(root.strip.label)
                    }
                }
            }
        }

        // Band 4 — the routing bank: one lamp per bus, physical buses on the
        // first row and virtual buses on the second, the A-over-B layout the
        // reference console prints. Together with the other strips' banks
        // this is the routing matrix.
        Item {
            implicitHeight: root.routingHeight
            Tk.Flex.shrink: 0

            Tk.Flex {
                anchors.fill: parent
                direction: Tk.Flex.Column
                gap: 2

                Repeater {
                    // Two passes over the bus list: physical first, virtual
                    // second. The rows keep their slots even when a kind is
                    // absent so every strip's desk band stays level.
                    model: 2
                    delegate: Tk.Flex {
                        id: busRow
                        required property int index
                        readonly property var rowBuses: {
                            const wantVirtual = busRow.index === 1
                            const rows = []
                            for (const bus of root.buses) {
                                if ((bus.virtual ?? false) === wantVirtual)
                                    rows.push(bus)
                            }
                            return rows
                        }
                        gap: 2

                        Repeater {
                            model: busRow.rowBuses
                            AudioConsolePad {
                                id: sendPad
                                required property var modelData
                                Tk.Flex.grow: 1
                                implicitHeight: 18
                                objectName: "consoleSend_" + root.strip.id + "_" + sendPad.modelData.index
                                text: sendPad.modelData.label
                                checkable: true
                                available: root.enabledControls
                                Binding on checked {
                                    when: !sendPad.down
                                    value: {
                                        for (const send of root.strip.sends) {
                                            if (send.busIndex === sendPad.modelData.index)
                                                return send.enabled
                                        }
                                        return false
                                    }
                                }
                                onToggled: {
                                    // The send's existing gain is sent back
                                    // unchanged, so toggling a cell never
                                    // silently resets the level the user
                                    // dialled in for it.
                                    let gain = 0.0
                                    for (const send of root.strip.sends) {
                                        if (send.busIndex === sendPad.modelData.index)
                                            gain = send.gainDb
                                    }
                                    root.model.setStripSend(root.strip.id, sendPad.modelData.index,
                                                            checked, gain)
                                }
                                Accessible.name: qsTr("Send %1 to %2")
                                    .arg(root.strip.label).arg(modelData.label)
                            }
                        }
                    }
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
                    objectName: "consoleStripMeter_" + root.strip.id
                    implicitWidth: 14
                    implicitHeight: root.deskHeight
                    reading: root.model.consoleLevels[root.strip.id]
                    Tk.Flex.shrink: 0
                }

                AudioConsoleFader {
                    objectName: "consoleStripFader_" + root.strip.id
                    implicitHeight: root.deskHeight
                    Tk.Flex.grow: 1
                    model: root.model
                    faderPosition: root.strip.faderPosition
                    enabledControl: root.enabledControls
                    accessibleName: qsTr("%1 level").arg(root.strip.label)
                    onMoved: position => root.model.setStripFader(root.strip.id, position)
                }
            }
        }

        // Band 6 — actions. The rack (ADR-0179) folds away at desk level.
        Item {
            implicitHeight: root.actionHeight
            Tk.Flex.shrink: 0

            AudioConsolePad {
                id: rackPad
                objectName: "consoleRackToggle_" + root.strip.id
                anchors.fill: parent
                text: qsTr("Rack")
                checkable: true
                Binding on checked { value: root.rackVisible; when: !rackPad.down }
                available: root.enabledControls
                onClicked: root.rackToggled()
                Accessible.name: qsTr("Processing rack for %1").arg(root.strip.label)
            }
        }
    }
}
