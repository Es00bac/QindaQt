// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor

// The figures that put the traces in context: what the queue looks like, how
// fast the clock is, how long the machine has been up, and what it is.
Tk.Flex {
    id: summary

    property var snapshot: ({})
    property int coreCount: 1

    gap: Tk.Theme.space.lg
    wrap: Tk.Flex.Wrap

    Readout {
        label: qsTr("Load average")
        value: Facade.number(summary.snapshot.load1, 2) + "  "
               + Facade.number(summary.snapshot.load5, 2) + "  "
               + Facade.number(summary.snapshot.load15, 2)
        ramp: Tk.Theme.ramp.load
        // One runnable task per logical CPU is where the queue stops being
        // slack, so that is where the ramp turns.
        rampValue: Facade.known(summary.snapshot.load1) ? summary.snapshot.load1 : 0
        rampTo: Math.max(1, summary.coreCount)
    }
    Readout {
        label: qsTr("Clock")
        value: Facade.known(Facade.hardware.frequencyMHz)
               ? (Facade.hardware.frequencyMHz / 1000).toFixed(2) + " GHz"
               : Facade.unavailable()
    }
    Readout {
        label: qsTr("Uptime")
        value: Facade.duration(summary.snapshot.uptime)
    }
    Readout {
        label: qsTr("Processes")
        value: Processes.totalCount + ""
    }
    Readout {
        Tk.Flex.grow: 1
        Tk.Flex.minWidth: 0
        label: qsTr("Processor")
        value: Facade.hardware.cpuModel !== undefined
               ? Facade.hardware.cpuModel : Facade.unavailable()
    }
}
