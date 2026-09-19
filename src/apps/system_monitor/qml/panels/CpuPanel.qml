// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Processor: the whole-machine trace, every logical CPU, and the figures
// that put them in context.
Parts.MonitorPanel {
    id: panel

    property var snapshot: Monitor.snapshot
    readonly property var cores: snapshot.cores !== undefined ? snapshot.cores : []
    readonly property real usage: Facade.known(snapshot.cpu) ? snapshot.cpu : 0

    // Per-core temperatures are sensor readings, not procfs counters, so they
    // arrive on the hardware cadence and are matched to cores by label.
    readonly property var temperatures: {
        const out = {}
        const sensors = Facade.hardware.sensors
        if (sensors === undefined) {
            return out
        }
        for (let i = 0; i < sensors.length; ++i) {
            const sensor = sensors[i]
            if (sensor.kind !== "temperature") {
                continue
            }
            // "Core 12", "Tccd1", "temp3" -- take the trailing integer.
            const match = /(\d+)\s*$/.exec(sensor.name)
            if (match !== null) {
                out[parseInt(match[1], 10)] = sensor.value
            }
        }
        return out
    }

    panelId: "cpu"
    title: qsTr("Processor")
    iconName: "cpu"
    summary: Facade.percent(panel.snapshot.cpu, 0)
    summaryColor: Tk.Theme.ramp.load.forValue(panel.usage, 0, 100)

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Parts.TraceGraph {
            // A fixed band rather than a share: the trace stops being more
            // legible past about ninety pixels, while the core list keeps
            // earning every row it is given.
            Tk.Flex.shrink: 0
            implicitHeight: 78
            maxValue: 100

            Tk.GraphSeries {
                id: cpuSeries
                ramp: Tk.Theme.ramp.load
                fillOpacity: 0.55
                lineWidth: 1.5
            }
        }

        Parts.CoreGrid {
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            cores: panel.cores
            temperatures: panel.temperatures
        }

        Tk.Divider {}

        Parts.CpuSummary {
            Tk.Flex.shrink: 0
            snapshot: panel.snapshot
            coreCount: panel.cores.length
        }
    }

    // Seed from the retained history so a freshly opened panel shows the last
    // few minutes rather than filling in from the right over two minutes.
    Component.onCompleted: {
        const history = Monitor.snapshot.history
        if (history === undefined) {
            return
        }
        for (let i = 0; i < history.length; ++i) {
            cpuSeries.append(Facade.known(history[i].cpu) ? history[i].cpu : 0)
        }
    }

    Connections {
        target: Monitor
        function onUpdated() {
            if (!Monitor.paused) {
                cpuSeries.append(panel.usage)
            }
        }
    }
}
