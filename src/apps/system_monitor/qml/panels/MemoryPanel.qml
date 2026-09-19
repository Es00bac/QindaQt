// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Memory: what is committed, what is still available, what the kernel is
// holding as cache, and swap. Used and available are shown as separate bars
// rather than one, because on Linux they are not complements -- cache counts
// as used but is available on demand, and reading one bar as the other is
// the most common way people misdiagnose a machine.
Parts.MonitorPanel {
    id: panel

    property var snapshot: Monitor.snapshot
    readonly property var memory: snapshot.memory !== undefined ? snapshot.memory : ({})
    readonly property real total: Facade.known(memory.total) ? memory.total : 0
    readonly property real usedPercent:
        total > 0 ? 100 * (Facade.known(memory.used) ? memory.used : 0) / total : 0
    readonly property real swapTotal: Facade.known(memory.swapTotal) ? memory.swapTotal : 0
    readonly property real swapPercent:
        swapTotal > 0 ? 100 * (Facade.known(memory.swapUsed) ? memory.swapUsed : 0) / swapTotal : 0

    panelId: "memory"
    title: qsTr("Memory")
    iconName: "memory-stick"
    summary: Facade.bytes(panel.memory.used) + " / " + Facade.bytes(panel.memory.total)
    summaryColor: Tk.Theme.ramp.memory.forValue(panel.usedPercent, 0, 100)

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Parts.TraceGraph {
            Tk.Flex.grow: 1
            Tk.Flex.minHeight: Tk.Theme.size.graphMinHeight
            Tk.Flex.maxHeight: 120
            maxValue: 100

            Tk.GraphSeries {
                id: memorySeries
                ramp: Tk.Theme.ramp.memory
                fillOpacity: 0.5
                lineWidth: 1.5
            }
        }

        Tk.Flex {
            direction: Tk.Flex.Column
            gap: 0

            Parts.MeterRow {
                label: qsTr("Used")
                labelWidth: 68
                valueWidth: 78
                amount: panel.usedPercent
                value: Facade.bytes(panel.memory.used)
                ramp: Tk.Theme.ramp.memory
            }
            Parts.MeterRow {
                label: qsTr("Cached")
                labelWidth: 68
                valueWidth: 78
                amount: panel.total > 0 && Facade.known(panel.memory.cached)
                        ? 100 * panel.memory.cached / panel.total : 0
                value: Facade.bytes(panel.memory.cached)
                ramp: Tk.Theme.ramp.neutral
            }
            Parts.MeterRow {
                label: qsTr("Available")
                labelWidth: 68
                valueWidth: 78
                amount: panel.total > 0 && Facade.known(panel.memory.available)
                        ? 100 * panel.memory.available / panel.total : 0
                value: Facade.bytes(panel.memory.available)
                ramp: Tk.Theme.ramp.neutral
            }
            Parts.MeterRow {
                visible: panel.swapTotal > 0
                label: qsTr("Swap")
                labelWidth: 68
                valueWidth: 78
                amount: panel.swapPercent
                value: Facade.bytes(panel.memory.swapUsed)
                ramp: Tk.Theme.ramp.memory
            }
        }

        Tk.Flex {
            gap: Tk.Theme.space.lg

            Parts.Readout {
                label: qsTr("Total")
                value: Facade.bytes(panel.memory.total)
                emphasis: true
            }
            Parts.Readout {
                label: qsTr("Swap total")
                value: panel.swapTotal > 0 ? Facade.bytes(panel.memory.swapTotal)
                                           : qsTr("none")
            }
        }
    }

    Component.onCompleted: {
        const history = Monitor.snapshot.history
        if (history === undefined) {
            return
        }
        for (let i = 0; i < history.length; ++i) {
            memorySeries.append(Facade.known(history[i].memory) ? history[i].memory : 0)
        }
    }

    Connections {
        target: Monitor
        function onUpdated() {
            if (!Monitor.paused) {
                memorySeries.append(panel.usedPercent)
            }
        }
    }
}
