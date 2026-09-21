// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// A physical bus's rack (ADR-0180): a three-band equalizer. The channel mode
// lives on the bus card's face (AudioConsoleBus), where the reference console
// puts it; this band is only the EQ. Each control sends the whole rack with
// one value changed.
Tk.Flex {
    id: rack

    required property var model
    required property var bus
    required property bool enabledControls

    readonly property var processing: bus.processing ?? ({})
    readonly property var equalizer: processing.equalizer ?? ({})
    readonly property bool eqOn: equalizer.enabled === true

    gap: Tk.Theme.space.md
    objectName: "consoleBusRack_" + bus.id

    function sendEq(key, value) {
        const next = { equalizer: {} }
        next.equalizer[key] = value
        rack.model.setBusProcessing(rack.bus.id, next)
    }

    AudioConsolePad {
        id: eqPad
        implicitHeight: 18
        objectName: "consoleBusRack_" + rack.bus.id + "_equalizer"
        text: qsTr("Equalizer")
        checkable: true
        available: rack.enabledControls
        Binding on checked { value: rack.eqOn; when: !eqPad.down }
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
}
