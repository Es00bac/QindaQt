// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// A physical bus's rack (ADR-0180): a three-band equalizer, and a delay
// stage (ADR-0288's dated addendum) that lines this bus's output up in time
// against another output or a VBAN peer fed from the same console. The
// channel mode lives on the bus card's face (AudioConsoleBus), where the
// reference console puts it; this band holds the EQ and the delay. Each
// control sends the whole rack with one value changed.
//
// AGENT-NOTE: delay is scoped to physical buses only, like the EQ above it -
// this component is never instantiated for a virtual bus (AudioConsoleBus.qml
// gates its "Rack" toggle on `!bus.virtual`), matching the graph, which never
// builds a rack for one either (ADR-0180,
// aBusRackNeedsItsOwnSinkAndOnlyOnAPhysicalBus). A virtual bus's own sink
// would have to become a post-rack node to gain one, which is unrelated,
// larger surgery than a delay control.
Tk.Flex {
    id: rack

    required property var model
    required property var bus
    required property bool enabledControls

    readonly property var processing: bus.processing ?? ({})
    readonly property var equalizer: processing.equalizer ?? ({})
    readonly property bool eqOn: equalizer.enabled === true
    readonly property int delayMs: processing.delayMs ?? 0

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

    // The delay stage: whole milliseconds, 0..1000 (audio_limits.h), sent
    // through the same whole-rack request path as the EQ above, like
    // AudioLatencyControl.qml's device-latency field.
    Tk.Flex {
        id: delayGroup
        gap: Tk.Theme.space.xs

        Tk.Label {
            text: qsTr("Delay")
            muted: true
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        Tk.NumberField {
            id: delayField
            objectName: "consoleBusRack_" + rack.bus.id + "_delay"
            small: true
            decimals: 0
            stepSize: 5
            scrub: false
            suffix: qsTr("ms")
            from: 0
            to: 1000
            value: rack.delayMs
            enabled: rack.enabledControls
            tooltip: qsTr("Delay bus %1 in milliseconds").arg(rack.bus.label)
            onValueModified: next => rack.model.setBusProcessing(
                rack.bus.id, { delayMs: Math.round(next) })
        }
        Tk.Button {
            id: delayReset
            objectName: "consoleBusRack_" + rack.bus.id + "_delayReset"
            small: true
            text: qsTr("Reset")
            available: rack.enabledControls && rack.delayMs !== 0
            tooltip: qsTr("Reset bus %1 delay to 0 milliseconds").arg(rack.bus.label)
            onClicked: rack.model.setBusProcessing(rack.bus.id, { delayMs: 0 })
        }
    }
}
