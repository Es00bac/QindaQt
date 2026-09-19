// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Storage: how full each filesystem is, and how hard each device is being
// worked. Space and traffic are different questions about different things
// -- a mount point and a block device -- so they are shown as two lists
// rather than merged into rows that would imply a mapping that is not there.
Parts.MonitorPanel {
    id: panel

    property var snapshot: Monitor.snapshot
    // AGENT-NOTE: the collector reports every local mount, including the
    // pseudo-filesystems (/proc, /sys, /dev/pts, cgroups) that have no
    // capacity, plus a crop of one-megabyte credential and firmware mounts.
    // Twenty such rows bury the four mounts a person actually has to keep an
    // eye on. The cutoff is capacity, not mount point, so it needs no list of
    // magic paths to stay correct -- and `showAllMounts` puts them all back
    // rather than deciding for the reader.
    readonly property var filesystems: {
        const all = snapshot.filesystems !== undefined ? snapshot.filesystems : []
        const real = []
        for (let i = 0; i < all.length; ++i) {
            if (panel.showAllMounts
                || (Facade.known(all[i].total) && all[i].total >= 64 * 1024 * 1024)) {
                real.push(all[i])
            }
        }
        return real
    }
    readonly property var disks: snapshot.disks !== undefined ? snapshot.disks : []

    readonly property real totalRead: {
        let sum = 0
        for (let i = 0; i < panel.disks.length; ++i) {
            if (Facade.known(panel.disks[i].readRate)) {
                sum += panel.disks[i].readRate
            }
        }
        return sum
    }
    readonly property real totalWrite: {
        let sum = 0
        for (let i = 0; i < panel.disks.length; ++i) {
            if (Facade.known(panel.disks[i].writeRate)) {
                sum += panel.disks[i].writeRate
            }
        }
        return sum
    }

    property bool showAllMounts: false

    panelId: "disks"
    title: qsTr("Storage")
    iconName: "hard-drive"
    summary: "R " + Facade.rate(panel.totalRead) + "   W " + Facade.rate(panel.totalWrite)

    controls: Tk.IconButton {
        objectName: "allMountsToggle"
        iconName: "eye"
        small: true
        checkable: true
        checked: panel.showAllMounts
        tooltip: qsTr("Also show pseudo-filesystems and tiny mounts")
        onToggled: panel.showAllMounts = checked
    }

    Tk.Scroll {
        anchors.fill: parent

        Tk.Flex {
            width: parent.width
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm

            Tk.SectionHeader {
                width: parent.width
                title: qsTr("Filesystems")
                count: panel.filesystems.length + ""
            }

            Repeater {
                model: panel.filesystems
                delegate: Parts.MeterRow {
                    id: fs
                    required property var modelData
                    readonly property real used:
                        Facade.known(fs.modelData.total) && fs.modelData.total > 0
                        ? 100 * fs.modelData.used / fs.modelData.total : 0

                    width: parent.width
                    label: fs.modelData.path
                    labelWidth: 110
                    valueWidth: 96
                    amount: fs.used
                    // The figure is what is LEFT, because that is the number
                    // a full disk makes you want; the bar already shows used.
                    value: Facade.bytes(fs.modelData.available) + qsTr(" free")
                    ramp: Tk.Theme.ramp.memory

                    trailing: Tk.Mono {
                        text: Facade.percent(fs.used, 0)
                        width: 38
                        horizontalAlignment: Text.AlignRight
                        color: Tk.Theme.ramp.memory.forValue(fs.used, 0, 100)
                    }
                }
            }

            Tk.SectionHeader {
                width: parent.width
                title: qsTr("Devices")
                count: panel.disks.length + ""
            }

            Repeater {
                model: panel.disks
                delegate: Tk.Flex {
                    id: device
                    required property var modelData
                    width: parent.width
                    align: Tk.Flex.Center
                    gap: Tk.Theme.space.sm
                    implicitHeight: Tk.Theme.size.row

                    Tk.Mono {
                        text: device.modelData.name
                        width: 84
                        elide: Text.ElideRight
                        color: Tk.Theme.color.textMuted
                        Tk.Flex.shrink: 0
                    }
                    Tk.Meter {
                        Tk.Flex.grow: 1
                        Tk.Flex.minWidth: 20
                        implicitHeight: Tk.Theme.size.meter
                        value: Facade.known(device.modelData.busy) ? device.modelData.busy : 0
                        ramp: Tk.Theme.ramp.io
                        radius: Tk.Theme.radius.xs
                        tooltip: qsTr("Time the device spent busy")
                    }
                    Tk.Mono {
                        text: "R " + Facade.rate(device.modelData.readRate)
                        width: 96
                        horizontalAlignment: Text.AlignRight
                        Tk.Flex.shrink: 0
                    }
                    Tk.Mono {
                        text: "W " + Facade.rate(device.modelData.writeRate)
                        width: 96
                        horizontalAlignment: Text.AlignRight
                        Tk.Flex.shrink: 0
                    }
                }
            }
        }
    }
}
