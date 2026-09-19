// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// A labelled figure: caption above, value below, in mono so a column of them
// lines up. `ramp` colours the value by magnitude so the number carries the
// same warning the bars do.
Tk.Flex {
    id: readout

    property string label: ""
    property string value: ""
    property var ramp: null
    property real rampValue: 0
    property real rampFrom: 0
    property real rampTo: 100
    property bool emphasis: false

    direction: Tk.Flex.Column
    gap: 0

    Tk.Caption {
        text: readout.label
        elide: Text.ElideRight
    }
    Tk.Mono {
        objectName: "readoutValue"
        text: readout.value
        font.pixelSize: readout.emphasis ? Tk.Theme.font.medium : Tk.Theme.font.small
        font.weight: readout.emphasis ? Font.DemiBold : Font.Normal
        color: readout.ramp !== null
               ? readout.ramp.forValue(readout.rampValue, readout.rampFrom, readout.rampTo)
               : Tk.Theme.color.text
    }
}
