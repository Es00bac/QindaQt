// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Network: received above the line, sent below it, on one shared auto-scaled
// axis so the two directions are directly comparable -- which is the whole
// question when something is saturating a link.
Parts.MonitorPanel {
    id: panel

    property var snapshot: Monitor.snapshot
    readonly property var interfaces: snapshot.network !== undefined ? snapshot.network : []
    property string selected: ""

    readonly property var current: {
        for (let i = 0; i < panel.interfaces.length; ++i) {
            if (panel.interfaces[i].name === panel.selected) {
                return panel.interfaces[i]
            }
        }
        // AGENT-NOTE: never default to the first interface -- that is `lo`
        // on every Linux machine, and a monitor that opens on the loopback
        // counter answers a question nobody asked. Pick the one that has
        // actually carried the most traffic.
        let best = ({})
        let bestBytes = -1
        for (let j = 0; j < panel.interfaces.length; ++j) {
            const candidate = panel.interfaces[j]
            if (candidate.name === "lo") {
                continue
            }
            const carried = (Facade.known(candidate.rxBytes) ? candidate.rxBytes : 0)
                          + (Facade.known(candidate.txBytes) ? candidate.txBytes : 0)
            if (carried > bestBytes) {
                bestBytes = carried
                best = candidate
            }
        }
        if (best.name !== undefined) {
            return best
        }
        return panel.interfaces.length > 0 ? panel.interfaces[0] : ({})
    }

    panelId: "network"
    title: qsTr("Network")
    iconName: "network"
    summary: "▼ " + Facade.rate(panel.current.rxRate)
             + "   ▲ " + Facade.rate(panel.current.txRate)

    controls: Tk.ComboBox {
        objectName: "interfaceSelector"
        small: true
        implicitWidth: 110
        model: {
            const names = []
            for (let i = 0; i < panel.interfaces.length; ++i) {
                names.push(panel.interfaces[i].name)
            }
            return names
        }
        currentIndex: Math.max(0, model.indexOf(panel.current.name))
        tooltip: qsTr("Interface to watch")
        onActivated: function(index) { panel.selected = model[index] }
    }

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Tk.Flex {
            Tk.Flex.grow: 1
            Tk.Flex.minHeight: Tk.Theme.size.graphMinHeight
            direction: Tk.Flex.Column
            gap: 0

            // AGENT-GUARD: both traces must share one ceiling or the mirror
            // lies -- a 1 KiB/s upload would otherwise look the same size as
            // a 100 MiB/s download. `scale` is computed across both.
            Parts.TraceGraph {
                id: rxTrace
                Tk.Flex.grow: 1
                autoScale: false
                maxValue: panel.scale
                scaleFormat: "rate"
                cornerText: qsTr("received")
                Tk.GraphSeries {
                    id: rxSeries
                    ramp: Tk.Theme.ramp.network
                    fillOpacity: 0.5
                }
            }
            Parts.TraceGraph {
                id: txTrace
                Tk.Flex.grow: 1
                autoScale: false
                maxValue: panel.scale
                mirrored: true
                scaleFormat: ""
                cornerText: qsTr("sent")
                Tk.GraphSeries {
                    id: txSeries
                    color: Tk.Theme.color.warning
                    fillOpacity: 0.45
                }
            }
        }

        Tk.Flex {
            gap: Tk.Theme.space.lg
            wrap: Tk.Flex.Wrap

            Parts.Readout {
                label: qsTr("Download")
                value: Facade.rate(panel.current.rxRate)
                emphasis: true
            }
            Parts.Readout {
                label: qsTr("Upload")
                value: Facade.rate(panel.current.txRate)
                emphasis: true
            }
            Parts.Readout {
                label: qsTr("Received")
                value: Facade.bytes(panel.current.rxBytes)
            }
            Parts.Readout {
                label: qsTr("Sent")
                value: Facade.bytes(panel.current.txBytes)
            }
        }
    }

    // One ceiling for both directions, with headroom, never below a floor so
    // an idle link does not magnify its own noise.
    property real scale: Math.max(65536, Math.max(rxSeries.peak, txSeries.peak) * 1.2)

    Connections {
        target: Monitor
        function onUpdated() {
            if (Monitor.paused) {
                return
            }
            rxSeries.append(Facade.known(panel.current.rxRate) ? panel.current.rxRate : 0)
            txSeries.append(Facade.known(panel.current.txRate) ? panel.current.txRate : 0)
        }
    }
}
