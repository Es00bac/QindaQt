// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor

// Every logical CPU as its own bar, with the core temperature when the
// hardware reports one. As many columns as the width affords: the point of a
// core list is to see the whole processor at once, and a machine with
// twenty-four threads in two columns is twelve rows -- a scrollbar, not a
// glance.
Tk.Scroll {
    id: grid

    property var cores: []
    property var temperatures: ({})

    Tk.Grid {
        width: parent.width
        columns: grid.width > 1180 ? "1fr 1fr 1fr 1fr"
               : grid.width > 880 ? "1fr 1fr 1fr"
               : grid.width > 460 ? "1fr 1fr" : "1fr"
        columnGap: Tk.Theme.space.lg
        rowGap: 0

        Repeater {
            model: grid.cores
            delegate: MeterRow {
                id: core
                required property var modelData
                readonly property var temperature: grid.temperatures[core.modelData.id]

                label: "C" + core.modelData.id
                labelWidth: 26
                valueWidth: 40
                amount: Facade.known(core.modelData.usage) ? core.modelData.usage : 0
                value: Facade.percent(core.modelData.usage, 0)
                ramp: Tk.Theme.ramp.load
                segments: 20

                trailing: Tk.Mono {
                    visible: Facade.known(core.temperature)
                    text: Facade.number(core.temperature, 0) + "°"
                    width: 30
                    horizontalAlignment: Text.AlignRight
                    color: Tk.Theme.ramp.thermal.forValue(
                               core.temperature === undefined ? 0 : core.temperature, 30, 95)
                }
            }
        }
    }
}
