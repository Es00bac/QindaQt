// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor

// One graphics device. Every reading comes from a driver that may not expose
// it, so anything missing shows a dash: "this GPU reports no power draw" and
// "this GPU is drawing no power" are different statements about the machine.
Tk.Flex {
    id: card

    property var gpu: ({})
    readonly property real vramPercent:
        Facade.known(card.gpu.memoryTotal) && card.gpu.memoryTotal > 0
        ? 100 * card.gpu.memoryUsed / card.gpu.memoryTotal : 0

    direction: Tk.Flex.Column
    gap: 0

    Tk.SectionHeader {
        title: card.gpu.name !== undefined ? card.gpu.name : qsTr("Graphics")
        count: card.gpu.driver !== undefined ? card.gpu.driver : ""
    }
    MeterRow {
        label: qsTr("Busy")
        labelWidth: 76
        amount: Facade.known(card.gpu.busy) ? card.gpu.busy : 0
        value: Facade.percent(card.gpu.busy, 0)
        ramp: Tk.Theme.ramp.load
    }
    MeterRow {
        visible: Facade.known(card.gpu.memoryTotal)
        label: qsTr("Memory")
        labelWidth: 76
        valueWidth: 84
        amount: card.vramPercent
        value: Facade.bytes(card.gpu.memoryUsed)
        ramp: Tk.Theme.ramp.memory
    }
    Tk.Flex {
        gap: Tk.Theme.space.lg
        wrap: Tk.Flex.Wrap

        Readout {
            label: qsTr("Temperature")
            value: Facade.known(card.gpu.temperature)
                   ? Facade.number(card.gpu.temperature, 0) + "°C"
                   : Facade.unavailable()
            ramp: Tk.Theme.ramp.thermal
            rampValue: Facade.known(card.gpu.temperature) ? card.gpu.temperature : 0
            rampFrom: 30
            rampTo: 95
        }
        Readout {
            label: qsTr("Power")
            value: Facade.known(card.gpu.power)
                   ? Facade.number(card.gpu.power, 1) + " W" : Facade.unavailable()
        }
        Readout {
            label: qsTr("Memory total")
            value: Facade.bytes(card.gpu.memoryTotal)
        }
    }
}
