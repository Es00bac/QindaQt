// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// A Tk.Graph dressed for this application: grid, baseline, and a scale label
// in the corner so an auto-scaled plot says what its ceiling currently is --
// without it a network graph is a shape with no magnitude.
Item {
    id: trace

    property alias graph: plot
    property alias capacity: plot.capacity
    property alias maxValue: plot.maxValue
    property alias autoScale: plot.autoScale
    property alias autoScaleFloor: plot.autoScaleFloor
    property alias mirrored: plot.mirrored
    // AGENT-GUARD: this MUST be the default property. Without it a
    // `Tk.GraphSeries {}` written inside a TraceGraph block lands in
    // Item.data instead of the plot's series list, and the graph draws
    // nothing at all while looking perfectly well-formed.
    default property alias series: plot.series
    // "percent" prints 100%, "rate" prints 3.3 MiB/s, "" hides the label.
    property string scaleFormat: "percent"
    property string cornerText: ""

    implicitWidth: Tk.Theme.size.panelMinWidth
    implicitHeight: Tk.Theme.size.graphMinHeight

    Tk.Graph {
        id: plot
        anchors.fill: parent
        capacity: 120
        maxValue: 100
        gridRows: 3
        gridColor: Tk.Theme.color.divider
        baselineColor: Tk.Theme.color.border
    }

    Tk.Caption {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 2
        visible: trace.scaleFormat.length > 0
        text: trace.scaleFormat === "rate"
              ? Facade.rate(plot.effectiveMax)
              : Facade.percent(plot.effectiveMax, 0)
        color: Tk.Theme.color.textDisabled
    }
    Tk.Caption {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 2
        visible: trace.cornerText.length > 0
        text: trace.cornerText
        color: Tk.Theme.color.textDisabled
    }
}
