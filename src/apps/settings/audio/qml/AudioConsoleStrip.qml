// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// One input channel strip (ADR-0173): a vertical fader with a dB legend, the
// mute/solo/mono buttons, a pan control, and one assignment button per bus.
//
// AGENT-CONTRACT: the fader is driven by POSITION and converted through the
// model's gain law, never by mapping dB linearly onto the slider. The legend
// beside it reads the same conversion, so the number and the knob cannot
// disagree.
ColumnLayout {
    id: root

    required property var model
    required property var strip
    required property var buses
    required property bool soloActive
    required property bool enabledControls

    spacing: Tokens.space["2"]
    objectName: "consoleStrip_" + strip.id
    // Owned here rather than on the toggle so a model republish that rebuilds
    // the row keeps the rack open.
    property bool rackVisible: false

    Label {
        Layout.fillWidth: true
        text: root.strip.label
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        // A strip whose device is absent stays usable; it is dimmed rather
        // than removed so the user's routing does not vanish with the device.
        opacity: root.strip.bound ? 1.0 : 0.55
        Accessible.name: root.strip.bound
            ? text : qsTr("%1, no device connected").arg(text)
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: Tokens.space["1"]

        T.Slider {
            id: fader
            objectName: "consoleStripFader_" + root.strip.id
            orientation: Qt.Vertical
            implicitHeight: 160
            from: 0.0
            to: 1.0
            enabled: root.enabledControls
            value: root.strip.faderPosition
            onMoved: root.model.setStripFader(root.strip.id, value)
            Accessible.name: qsTr("%1 level").arg(root.strip.label)
            Accessible.description: qsTr("%1 decibels")
                .arg(Math.round(root.model.gainForFaderPosition(value) * 10) / 10)

            background: Rectangle {
                x: fader.leftPadding + fader.availableWidth / 2 - width / 2
                y: fader.topPadding
                implicitWidth: 6
                width: implicitWidth
                height: fader.availableHeight
                radius: 3
                color: Tokens.bg.raised
                // The unity mark: a console operator finds 0 dB by eye, so the
                // scale draws it rather than leaving the fader unlabelled.
                Rectangle {
                    width: 14
                    height: 2
                    x: -4
                    y: fader.availableHeight * (1.0 - root.model.unityFaderPosition())
                    color: Tokens.outline.strong
                }
            }
        }

        // Meter. Read from the live level channel, not from the strip row:
        // rows change only when the console is reconfigured.
        AudioConsoleMeter {
            objectName: "consoleStripMeter_" + root.strip.id
            reading: root.model.consoleLevels[root.strip.id]
        }
    }

    // The rack (ADR-0179) folds away: a console strip is narrow, and four
    // blocks of dials only matter while the user is shaping this input.
    Button {
        objectName: "consoleRackToggle_" + root.strip.id
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Rack")
        checkable: true
        checked: root.rackVisible
        onToggled: root.rackVisible = checked
        Accessible.name: qsTr("Processing rack for %1").arg(root.strip.label)
    }
    Loader {
        Layout.fillWidth: true
        active: root.rackVisible
        visible: active
        sourceComponent: AudioConsoleRack {
            model: root.model
            strip: root.strip
            enabledControls: root.enabledControls
        }
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("%1 dB").arg(Math.round(root.strip.gainDb * 10) / 10)
        font: Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.caption })
        Accessible.ignored: true
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
        Layout.alignment: Qt.AlignHCenter
        spacing: Tokens.space["1"]

        Button {
            objectName: "consoleStripMute_" + root.strip.id
            text: qsTr("M")
            checkable: true
            checked: root.strip.muted
            enabled: root.enabledControls
            onToggled: root.model.setStripMuted(root.strip.id, checked)
            Accessible.name: qsTr("Mute %1").arg(root.strip.label)
        }
        Button {
            objectName: "consoleStripSolo_" + root.strip.id
            text: qsTr("S")
            checkable: true
            checked: root.strip.soloed
            enabled: root.enabledControls
            onToggled: root.model.setStripSoloed(root.strip.id, checked)
            Accessible.name: qsTr("Solo %1").arg(root.strip.label)
        }
        Button {
            objectName: "consoleStripMono_" + root.strip.id
            text: qsTr("Mono")
            checkable: true
            checked: root.strip.mono
            enabled: root.enabledControls
            onToggled: root.model.setStripMono(root.strip.id, checked)
            Accessible.name: qsTr("Mono %1").arg(root.strip.label)
        }
    }

    // The matrix column for this strip: one button per bus, which together
    // with the other strips' columns is the routing matrix.
    GridLayout {
        Layout.alignment: Qt.AlignHCenter
        columns: 4
        columnSpacing: Tokens.space["1"]
        rowSpacing: Tokens.space["1"]

        Repeater {
            model: root.buses
            Button {
                required property var modelData
                objectName: "consoleSend_" + root.strip.id + "_" + modelData.index
                text: modelData.label
                checkable: true
                enabled: root.enabledControls
                checked: {
                    for (const send of root.strip.sends) {
                        if (send.busIndex === modelData.index)
                            return send.enabled
                    }
                    return false
                }
                onToggled: {
                    // The send's existing gain is sent back unchanged, so
                    // toggling a cell never silently resets the level the user
                    // dialled in for it.
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
    }
}
