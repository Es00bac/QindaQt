// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One input channel strip (ADR-0173), condensed to a narrow card: name and
// source on top, then a desk row of meter, fader, and the routing bank
// (one pad per bus, mono/solo/mute below). Cards flow left-to-right and wrap
// (AudioConsoleSection), so the console uses whatever width the window offers
// and never scrolls horizontally.
//
// AGENT-CONTRACT: the fader is driven by POSITION and converted through the
// model's gain law, never by mapping dB linearly onto the slot. The handle
// readout reads the same conversion, so the number and the slot cannot
// disagree.
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

        RowLayout {
            Layout.fillWidth: true
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

            // The rack (ADR-0179) folds away at desk level: four blocks of
            // knobs only matter while the user is shaping this input, and an
            // always-open rack would double every card's height.
            AudioConsolePad {
                objectName: "consoleRackToggle_" + root.strip.id
                text: qsTr("Rack")
                checkable: true
                checked: root.rackVisible
                available: root.enabledControls
                onClicked: root.rackToggled()
                Accessible.name: qsTr("Processing rack for %1").arg(root.strip.label)
            }
        }

        // Which microphone this strip follows (ADR-0178). "Automatic" lets the
        // service choose; picking a device pins it, and a pinned device that is
        // unplugged keeps the strip unbound rather than handing it another one.
        AudioConsoleDevicePicker {
            objectName: "consoleStripSource_" + root.strip.id
            Layout.fillWidth: true
            visible: !root.strip.virtual
            devices: root.model.inputDevices ?? []
            boundSerial: root.strip.sourceSerial ?? 0
            pinned: root.strip.pinned ?? false
            enabled: root.enabledControls
            accessibleName: qsTr("Input for %1").arg(root.strip.label)
            onPicked: serial => root.model.setStripSource(root.strip.id, serial)
        }

        RowLayout {
            Layout.fillWidth: true
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
    }
}
