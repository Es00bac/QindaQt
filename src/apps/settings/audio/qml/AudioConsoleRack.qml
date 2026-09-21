// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaTK as Tk

// A strip's processing rack (ADR-0179): gate, compressor, equalizer, limiter,
// in the order the graph applies them. Every control sends the WHOLE rack with
// one value changed; the service judges it whole and refuses it whole.
//
// The condensed console renders the rack as a full-width band of rotary
// knobs — the desk idiom for "set and forget" parameters. Blocks flow and
// wrap so the band never forces a horizontal scroll at narrow widths.
//
// AGENT-NOTE: each block repeats its pad+knobs rather than sharing a
// delegate, because each knob binds a different key of the projected
// processing map; a Repeater over keys would lose the per-key defaults,
// units, and ranges that are the actual contract with Audio1.
Tk.Flex {
    id: rack

    required property var model
    required property var strip
    required property bool enabledControls

    readonly property var processing: strip.processing ?? ({})

    wrap: Tk.Flex.Wrap
    gap: Tk.Theme.space.lg
    objectName: "consoleRack_" + strip.id

    function send(block, key, value) {
        const next = {}
        next[block] = {}
        next[block][key] = value
        rack.model.setStripProcessing(rack.strip.id, next)
    }

    component Block: Tk.Flex {
        id: block
        required property string blockName
        required property string title
        readonly property var settings: rack.processing[blockName] ?? ({})
        readonly property bool on: settings.enabled === true
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.xs

        AudioConsolePad {
            id: blockPad
            implicitHeight: 18
            objectName: "consoleRack_" + rack.strip.id + "_" + block.blockName
            text: block.title
            checkable: true
            available: rack.enabledControls
            Binding on checked { value: block.on; when: !blockPad.down }
            onToggled: rack.send(block.blockName, "enabled", checked)
            Accessible.name: qsTr("%1 for %2").arg(block.title).arg(rack.strip.label)
        }
    }

    Block {
        id: denoiserBlock
        blockName: "denoiser"
        title: qsTr("Denoiser")
        Tk.Flex {
            gap: Tk.Theme.space.xs
            AudioConsoleKnob { label: qsTr("Voice threshold"); unit: " %"; from: 0; to: 100
                value: denoiserBlock.settings.vadThreshold ?? 50
                enabledControl: denoiserBlock.on && rack.enabledControls
                onCommitted: v => rack.send("denoiser", "vadThreshold", v) }
        }
    }

    Block {
        id: gateBlock
        blockName: "gate"
        title: qsTr("Gate")
        Tk.Flex {
            gap: Tk.Theme.space.xs
            AudioConsoleKnob { label: qsTr("Threshold"); unit: " dB"; from: -80; to: 0
                value: gateBlock.settings.thresholdDb ?? -40
                enabledControl: gateBlock.on && rack.enabledControls
                onCommitted: v => rack.send("gate", "thresholdDb", v) }
            AudioConsoleKnob { label: qsTr("Attack"); unit: " ms"; from: 0; to: 500
                value: gateBlock.settings.attackMs ?? 5
                enabledControl: gateBlock.on && rack.enabledControls
                onCommitted: v => rack.send("gate", "attackMs", v) }
            AudioConsoleKnob { label: qsTr("Release"); unit: " ms"; from: 0; to: 2000
                value: gateBlock.settings.releaseMs ?? 200
                enabledControl: gateBlock.on && rack.enabledControls
                onCommitted: v => rack.send("gate", "releaseMs", v) }
        }
    }

    Block {
        id: compressorBlock
        blockName: "compressor"
        title: qsTr("Compressor")
        Tk.Flex {
            gap: Tk.Theme.space.xs
            AudioConsoleKnob { label: qsTr("Threshold"); unit: " dB"; from: -80; to: 0
                value: compressorBlock.settings.thresholdDb ?? -18
                enabledControl: compressorBlock.on && rack.enabledControls
                onCommitted: v => rack.send("compressor", "thresholdDb", v) }
            AudioConsoleKnob { label: qsTr("Ratio"); unit: ":1"; from: 1; to: 20; decimals: 1
                value: compressorBlock.settings.ratio ?? 3
                enabledControl: compressorBlock.on && rack.enabledControls
                onCommitted: v => rack.send("compressor", "ratio", v) }
            AudioConsoleKnob { label: qsTr("Makeup"); unit: " dB"; from: 0; to: 24
                value: compressorBlock.settings.makeupDb ?? 0
                enabledControl: compressorBlock.on && rack.enabledControls
                onCommitted: v => rack.send("compressor", "makeupDb", v) }
        }
    }

    Block {
        id: equalizerBlock
        blockName: "equalizer"
        title: qsTr("Equalizer")
        Tk.Flex {
            gap: Tk.Theme.space.xs
            AudioConsoleKnob { label: qsTr("Low"); unit: " dB"; from: -24; to: 24
                value: equalizerBlock.settings.lowGainDb ?? 0
                enabledControl: equalizerBlock.on && rack.enabledControls
                onCommitted: v => rack.send("equalizer", "lowGainDb", v) }
            AudioConsoleKnob { label: qsTr("Mid"); unit: " dB"; from: -24; to: 24
                value: equalizerBlock.settings.midGainDb ?? 0
                enabledControl: equalizerBlock.on && rack.enabledControls
                onCommitted: v => rack.send("equalizer", "midGainDb", v) }
            AudioConsoleKnob { label: qsTr("High"); unit: " dB"; from: -24; to: 24
                value: equalizerBlock.settings.highGainDb ?? 0
                enabledControl: equalizerBlock.on && rack.enabledControls
                onCommitted: v => rack.send("equalizer", "highGainDb", v) }
        }
    }

    Block {
        id: limiterBlock
        blockName: "limiter"
        title: qsTr("Limiter")
        Tk.Flex {
            gap: Tk.Theme.space.xs
            AudioConsoleKnob { label: qsTr("Ceiling"); unit: " dB"; from: -20; to: 0; decimals: 1
                value: limiterBlock.settings.ceilingDb ?? -1
                enabledControl: limiterBlock.on && rack.enabledControls
                onCommitted: v => rack.send("limiter", "ceilingDb", v) }
        }
    }
}
