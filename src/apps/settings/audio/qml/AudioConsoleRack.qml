// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// A strip's processing rack (ADR-0179): gate, compressor, equalizer, limiter,
// in the order the graph applies them. Every control sends the WHOLE rack with
// one value changed; the service judges it whole and refuses it whole.
ColumnLayout {
    id: rack

    required property var model
    required property var strip
    required property bool enabledControls

    readonly property var processing: strip.processing ?? ({})
    spacing: Tokens.space["2"]
    objectName: "consoleRack_" + strip.id

    function send(block, key, value) {
        const next = {}
        next[block] = {}
        next[block][key] = value
        rack.model.setStripProcessing(rack.strip.id, next)
    }

    component Block: ColumnLayout {
        id: block
        required property string blockName
        required property string title
        readonly property var settings: rack.processing[blockName] ?? ({})
        readonly property bool on: settings.enabled === true
        spacing: Tokens.space["1"]
        Layout.fillWidth: true
        Switch {
            objectName: "consoleRack_" + rack.strip.id + "_" + block.blockName
            text: block.title
            checked: block.on
            enabled: rack.enabledControls
            onToggled: rack.send(block.blockName, "enabled", checked)
        }
    }

    Block {
        blockName: "denoiser"
        title: qsTr("Denoiser")
        AudioConsoleRackDial { label: qsTr("Voice threshold"); unit: " %"; from: 0; to: 100
            value: parent.settings.vadThreshold ?? 50; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("denoiser", "vadThreshold", v) }
    }

    Block {
        blockName: "gate"
        title: qsTr("Gate")
        AudioConsoleRackDial { label: qsTr("Threshold"); unit: " dB"; from: -80; to: 0
            value: parent.settings.thresholdDb ?? -40; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("gate", "thresholdDb", v) }
        AudioConsoleRackDial { label: qsTr("Attack"); unit: " ms"; from: 0; to: 500
            value: parent.settings.attackMs ?? 5; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("gate", "attackMs", v) }
        AudioConsoleRackDial { label: qsTr("Release"); unit: " ms"; from: 0; to: 2000
            value: parent.settings.releaseMs ?? 200; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("gate", "releaseMs", v) }
    }

    Block {
        blockName: "compressor"
        title: qsTr("Compressor")
        AudioConsoleRackDial { label: qsTr("Threshold"); unit: " dB"; from: -80; to: 0
            value: parent.settings.thresholdDb ?? -18; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("compressor", "thresholdDb", v) }
        AudioConsoleRackDial { label: qsTr("Ratio"); unit: ":1"; from: 1; to: 20; decimals: 1
            value: parent.settings.ratio ?? 3; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("compressor", "ratio", v) }
        AudioConsoleRackDial { label: qsTr("Makeup"); unit: " dB"; from: 0; to: 24
            value: parent.settings.makeupDb ?? 0; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("compressor", "makeupDb", v) }
    }

    Block {
        blockName: "equalizer"
        title: qsTr("Equalizer")
        AudioConsoleRackDial { label: qsTr("Low"); unit: " dB"; from: -24; to: 24
            value: parent.settings.lowGainDb ?? 0; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("equalizer", "lowGainDb", v) }
        AudioConsoleRackDial { label: qsTr("Mid"); unit: " dB"; from: -24; to: 24
            value: parent.settings.midGainDb ?? 0; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("equalizer", "midGainDb", v) }
        AudioConsoleRackDial { label: qsTr("High"); unit: " dB"; from: -24; to: 24
            value: parent.settings.highGainDb ?? 0; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("equalizer", "highGainDb", v) }
    }

    Block {
        blockName: "limiter"
        title: qsTr("Limiter")
        AudioConsoleRackDial { label: qsTr("Ceiling"); unit: " dB"; from: -20; to: 0; decimals: 1
            value: parent.settings.ceilingDb ?? -1; enabledControl: parent.on && rack.enabledControls
            onCommitted: v => rack.send("limiter", "ceilingDb", v) }
    }
}
