// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Processor: the whole-machine trace, every logical CPU, and the figures
// that put them in context -- model, current clock, load average, uptime.
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
            id: trace
            // A fixed band rather than a share: the trace stops being more
            // legible past about ninety pixels, while the core list keeps
            // earning every row it is given.
            Tk.Flex.shrink: 0
            implicitHeight: 78
            maxValue: 100
            cornerText: panel.snapshot.cpuModel !== undefined ? "" : ""

            Tk.GraphSeries {
                id: cpuSeries
                ramp: Tk.Theme.ramp.load
                fillOpacity: 0.55
                lineWidth: 1.5
            }
        }

        Tk.Scroll {
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0

            Tk.Grid {
                width: parent.width
                // As many columns as the width affords. The point of a core
                // list is to see the whole processor at once: a machine with
                // twenty-four threads in two columns is twelve rows, which is
                // a scrollbar, not a glance.
                columns: panel.width > 1180 ? "1fr 1fr 1fr 1fr"
                       : panel.width > 880 ? "1fr 1fr 1fr"
                       : panel.width > 460 ? "1fr 1fr" : "1fr"
                columnGap: Tk.Theme.space.lg
                rowGap: 0

                Repeater {
                    model: panel.cores
                    delegate: Parts.MeterRow {
                        id: core
                        required property var modelData
                        readonly property var temperature:
                            panel.temperatures[core.modelData.id]

                        label: "C" + core.modelData.id
                        labelWidth: 26
                        amount: Facade.known(core.modelData.usage) ? core.modelData.usage : 0
                        value: Facade.percent(core.modelData.usage, 0)
                        ramp: Tk.Theme.ramp.load
                        segments: 20
                        valueWidth: 40

                        trailing: Tk.Mono {
                            visible: Facade.known(core.temperature)
                            text: Facade.number(core.temperature, 0) + "°"
                            width: 30
                            horizontalAlignment: Text.AlignRight
                            color: Tk.Theme.ramp.thermal.forValue(
                                       core.temperature === undefined ? 0 : core.temperature,
                                       30, 95)
                        }
                    }
                }
            }
        }

        Tk.Divider {}

        Tk.Flex {
            gap: Tk.Theme.space.lg
            wrap: Tk.Flex.Wrap

            Parts.Readout {
                label: qsTr("Load average")
                value: Facade.number(panel.snapshot.load1, 2) + "  "
                       + Facade.number(panel.snapshot.load5, 2) + "  "
                       + Facade.number(panel.snapshot.load15, 2)
                ramp: Tk.Theme.ramp.load
                // One runnable task per logical CPU is the point where the
                // queue stops being slack, so that is where the ramp turns.
                rampValue: Facade.known(panel.snapshot.load1) ? panel.snapshot.load1 : 0
                rampTo: Math.max(1, panel.cores.length)
            }
            Parts.Readout {
                label: qsTr("Clock")
                value: Facade.known(Facade.hardware.frequencyMHz)
                       ? (Facade.hardware.frequencyMHz / 1000).toFixed(2) + " GHz"
                       : Facade.unavailable()
            }
            Parts.Readout {
                label: qsTr("Uptime")
                value: Facade.duration(panel.snapshot.uptime)
            }
            Parts.Readout {
                label: qsTr("Processes")
                value: Processes.totalCount + ""
            }
            Parts.Readout {
                Tk.Flex.grow: 1
                Tk.Flex.minWidth: 0
                label: qsTr("Processor")
                value: Facade.hardware.cpuModel !== undefined
                       ? Facade.hardware.cpuModel : Facade.unavailable()
            }
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
            if (Monitor.paused) {
                return
            }
            cpuSeries.append(panel.usage)
        }
    }
}
