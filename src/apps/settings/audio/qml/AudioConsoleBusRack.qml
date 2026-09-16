// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// A physical bus's rack (ADR-0180): a three-band equalizer and a channel
// mode. Each control sends the whole rack with one value changed.
RowLayout {
    id: rack

    required property var model
    required property var bus
    required property bool enabledControls

    readonly property var processing: bus.processing ?? ({})
    readonly property var equalizer: processing.equalizer ?? ({})
    readonly property bool eqOn: equalizer.enabled === true
    spacing: Tokens.space["3"]
    objectName: "consoleBusRack_" + bus.id

    function sendEq(key, value) {
        const next = { equalizer: {} }
        next.equalizer[key] = value
        rack.model.setBusProcessing(rack.bus.id, next)
    }

    AudioConsolePad {
        Layout.preferredHeight: 16
        objectName: "consoleBusRack_" + rack.bus.id + "_equalizer"
        text: qsTr("Equalizer")
        checkable: true
        checked: rack.eqOn
        available: rack.enabledControls
        onToggled: rack.sendEq("enabled", checked)
        Accessible.name: qsTr("Equalizer for bus %1").arg(rack.bus.label)
    }
    AudioConsoleKnob { label: qsTr("Low"); unit: " dB"; from: -24; to: 24
        value: rack.equalizer.lowGainDb ?? 0; enabledControl: rack.eqOn && rack.enabledControls
        onCommitted: v => rack.sendEq("lowGainDb", v) }
    AudioConsoleKnob { label: qsTr("Mid"); unit: " dB"; from: -24; to: 24
        value: rack.equalizer.midGainDb ?? 0; enabledControl: rack.eqOn && rack.enabledControls
        onCommitted: v => rack.sendEq("midGainDb", v) }
    AudioConsoleKnob { label: qsTr("High"); unit: " dB"; from: -24; to: 24
        value: rack.equalizer.highGainDb ?? 0; enabledControl: rack.eqOn && rack.enabledControls
        onCommitted: v => rack.sendEq("highGainDb", v) }

    // The channel mode: which channel of the mix reaches which side of the
    // device. The index is derived from the published mode, never stored.
    ComboBox {
        objectName: "consoleBusMode_" + rack.bus.id
        Layout.preferredWidth: 160
        enabled: rack.enabledControls
        textRole: "label"
        valueRole: "token"
        model: [
            { token: "normal", label: qsTr("Stereo") },
            { token: "swap", label: qsTr("Swap left and right") },
            { token: "left", label: qsTr("Left to both") },
            { token: "right", label: qsTr("Right to both") }
        ]
        currentIndex: {
            const token = rack.processing.mode ?? "normal"
            for (let index = 0; index < model.length; ++index) {
                if (model[index].token === token) {
                    return index
                }
            }
            return 0
        }
        onActivated: index => rack.model.setBusProcessing(rack.bus.id, { mode: model[index].token })
        Accessible.name: qsTr("Channel mode for bus %1").arg(rack.bus.label)
    }
}
