// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One input channel strip (ADR-0173), condensed to a narrow card: name, the
// device this strip follows, then a desk band of meter, fader, and the routing
// bank (one pad per bus, mono/solo/mute below), then the card's actions. Cards
// flow left-to-right and wrap (AudioConsoleSection), so the console uses
// whatever width the window offers and never scrolls horizontally.
//
// AGENT-CONTRACT: the fader is driven by POSITION and converted through the
// model's gain law, never by mapping dB linearly onto the slot. The handle
// readout reads the same conversion, so the number and the slot cannot
// disagree.
//
// AGENT-CONTRACT: the four band heights below are shared VERBATIM with
// AudioConsoleBus.qml, and a band that does not apply to a card keeps its slot
// rather than collapsing. Cards are read across a row like a real desk, so
// every meter, fader and pad must sit at the same height as its neighbour's.
// Before this rule, a virtual strip hid its device picker and floated its
// fader above every hardware strip in the same row, and buses put their picker
// at the bottom while strips put it at the top.
// qindaqt.settings-audio-console-alignment fails if they drift apart.
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
    readonly property int deskHeight: 150
    readonly property int actionHeight: 18

    width: 120
    implicitHeight: stripColumn.implicitHeight + Tokens.space["2"] * 2
    radius: Tokens.radius.s
    color: Tokens.bg.raised
    border.width: Tokens.space["1"] / 2
    border.color: Tokens.outline.divider
    objectName: "consoleStrip_" + strip.id

    // Hardware inputs take the accent colour, virtual inputs the warning
    // colour - the same convention the meter uses for its hot end, so the
    // stripe reads as "kind", not decoration.
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        anchors.rightMargin: 4
        anchors.leftMargin: 4
        height: 2
        radius: 1
        color: root.strip.virtual ? Tokens.status.warning.background : Tokens.accent.default
        opacity: 0.7
    }

    ColumnLayout {
        id: stripColumn

        anchors.fill: parent
        anchors.margins: Tokens.space["2"]
        spacing: Tokens.space["1"]

        // Band 1 — name.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.headerHeight
            spacing: Tokens.space["1"]

            Label {
                Layout.fillWidth: true
                text: root.strip.label
                font.weight: Font.DemiBold
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                // A strip whose device is absent stays usable; it is dimmed
                // rather than removed so the user's routing does not vanish
                // with the device.
                opacity: root.strip.bound ? 1.0 : 0.55
                Accessible.name: root.strip.bound
                    ? text : qsTr("%1, no device connected").arg(text)
            }

            // AGENT-NOTE: no kind caption here, deliberately. A 120 px card
            // cannot spend width on a word the card already says twice — the
            // stripe along the top edge is the kind colour, and a virtual
            // strip has nothing in its assignment band. The bus card keeps its
            // caption because "physical" and "virtual" buses differ in what
            // they DO, not just where their audio comes from. The band
            // geometry is what has to match, not the text in it.
        }

        // Band 2 — assignment. Which microphone this strip follows (ADR-0178).
        // "Automatic" lets the service choose; picking a device pins it, and a
        // pinned device that is unplugged keeps the strip unbound rather than
        // handing it another one. A virtual strip has nothing to assign, so
        // the slot is emptied rather than removed.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.assignmentHeight

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

        // Band 3 — the desk.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.deskHeight
            spacing: Tokens.space["1"]

            // Meter. Read from the live level channel, not from the strip row:
            // rows change only when the console is reconfigured.
            AudioConsoleMeter {
                objectName: "consoleStripMeter_" + root.strip.id
                Layout.preferredWidth: 12
                Layout.fillHeight: true
                reading: root.model.consoleLevels[root.strip.id]
            }

            AudioConsoleFader {
                objectName: "consoleStripFader_" + root.strip.id
                Layout.preferredWidth: 40
                Layout.fillHeight: true
                model: root.model
                faderPosition: root.strip.faderPosition
                enabledControl: root.enabledControls
                accessibleName: qsTr("%1 level").arg(root.strip.label)
                onMoved: position => root.model.setStripFader(root.strip.id, position)
            }

            // The matrix column for this strip: one pad per bus. Together with
            // the other strips' columns it is the routing matrix.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 1

                Repeater {
                    model: root.buses
                    AudioConsolePad {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 16
                        objectName: "consoleSend_" + root.strip.id + "_" + modelData.index
                        text: modelData.label
                        checkable: true
                        available: root.enabledControls
                        checked: {
                            for (const send of root.strip.sends) {
                                if (send.busIndex === modelData.index)
                                    return send.enabled
                            }
                            return false
                        }
                        onToggled: {
                            // The send's existing gain is sent back unchanged,
                            // so toggling a cell never silently resets the
                            // level the user dialled in for it.
                            let gain = 0.0
                            for (const send of root.strip.sends) {
                                if (send.busIndex === modelData.index)
                                    gain = send.gainDb
                            }
                            root.model.setStripSend(root.strip.id, modelData.index,
                                                    checked, gain)
                        }
                        Accessible.name: qsTr("Send %1 to %2")
                            .arg(root.strip.label).arg(modelData.label)
                    }
                }

                Item { Layout.fillWidth: true; Layout.fillHeight: true }

                AudioConsolePad {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    objectName: "consoleStripMono_" + root.strip.id
                    text: qsTr("mono")
                    checkable: true
                    checked: root.strip.mono
                    available: root.enabledControls
                    onToggled: root.model.setStripMono(root.strip.id, checked)
                    Accessible.name: qsTr("Mono %1").arg(root.strip.label)
                }
                AudioConsolePad {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    objectName: "consoleStripSolo_" + root.strip.id
                    text: qsTr("solo")
                    checkable: true
                    checked: root.strip.soloed
                    lampColor: Tokens.status.warning.background
                    lampTextColor: Tokens.status.success.foreground
                    available: root.enabledControls
                    onToggled: root.model.setStripSoloed(root.strip.id, checked)
                    Accessible.name: qsTr("Solo %1").arg(root.strip.label)
                }
                AudioConsolePad {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    objectName: "consoleStripMute_" + root.strip.id
                    text: qsTr("Mute")
                    checkable: true
                    checked: root.strip.muted
                    destructive: true
                    lampColor: Tokens.danger.default
                    available: root.enabledControls
                    onToggled: root.model.setStripMuted(root.strip.id, checked)
                    Accessible.name: qsTr("Mute %1").arg(root.strip.label)
                }
            }
        }

        // Band 4 — actions. The rack (ADR-0179) folds away at desk level: four
        // blocks of knobs only matter while the user is shaping this input,
        // and an always-open rack would double every card's height.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.actionHeight
            spacing: Tokens.space["1"]

            AudioConsolePad {
                objectName: "consoleRackToggle_" + root.strip.id
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("Rack")
                checkable: true
                checked: root.rackVisible
                available: root.enabledControls
                onClicked: root.rackToggled()
                Accessible.name: qsTr("Processing rack for %1").arg(root.strip.label)
            }
        }
    }
}
