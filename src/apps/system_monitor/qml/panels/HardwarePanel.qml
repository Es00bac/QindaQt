// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Graphics and sensors. Every reading here comes from a driver that may not
// expose it; anything missing is named as unavailable rather than shown as a
// zero, because "this GPU reports no power draw" and "this GPU is drawing no
// power" are different statements about the machine.
Parts.MonitorPanel {
    id: panel

    readonly property var gpus: Facade.hardware.gpus !== undefined ? Facade.hardware.gpus : []
    readonly property var sensors:
        Facade.hardware.sensors !== undefined ? Facade.hardware.sensors : []

    function sensorsOfKind(kind) {
        const out = []
        for (let i = 0; i < panel.sensors.length; ++i) {
            if (panel.sensors[i].kind === kind) {
                out.push(panel.sensors[i])
            }
        }
        return out
    }

    panelId: "hardware"
    title: qsTr("Hardware")
    iconName: "thermometer"
    summary: panel.gpus.length > 0 ? Facade.percent(panel.gpus[0].busy, 0) : ""

    Tk.Scroll {
        anchors.fill: parent

        Tk.Flex {
            width: parent.width
            direction: Tk.Flex.Column
            gap: Tk.Theme.space.sm

            Tk.EmptyState {
                width: parent.width
                visible: panel.gpus.length === 0 && panel.sensors.length === 0
                iconName: "thermometer"
                title: Facade.hardwareAvailable ? qsTr("No GPU or sensor readings")
                                                : qsTr("Reading hardware…")
                text: Facade.hardwareAvailable
                      ? qsTr("This machine exposes no supported GPU counters or hwmon sensors.")
                      : ""
            }

            Repeater {
                model: panel.gpus
                delegate: Tk.Flex {
                    id: gpu
                    required property var modelData
                    readonly property real vramPercent:
                        Facade.known(gpu.modelData.memoryTotal) && gpu.modelData.memoryTotal > 0
                        ? 100 * gpu.modelData.memoryUsed / gpu.modelData.memoryTotal : 0

                    width: parent.width
                    direction: Tk.Flex.Column
                    gap: 0

                    Tk.SectionHeader {
                        width: parent.width
                        title: gpu.modelData.name !== undefined ? gpu.modelData.name : qsTr("Graphics")
                        count: gpu.modelData.driver !== undefined ? gpu.modelData.driver : ""
                    }
                    Parts.MeterRow {
                        width: parent.width
                        label: qsTr("Busy")
                        labelWidth: 76
                        amount: Facade.known(gpu.modelData.busy) ? gpu.modelData.busy : 0
                        value: Facade.percent(gpu.modelData.busy, 0)
                        ramp: Tk.Theme.ramp.load
                    }
                    Parts.MeterRow {
                        width: parent.width
                        visible: Facade.known(gpu.modelData.memoryTotal)
                        label: qsTr("Memory")
                        labelWidth: 76
                        valueWidth: 84
                        amount: gpu.vramPercent
                        value: Facade.bytes(gpu.modelData.memoryUsed)
                        ramp: Tk.Theme.ramp.memory
                    }
                    Tk.Flex {
                        width: parent.width
                        gap: Tk.Theme.space.lg
                        wrap: Tk.Flex.Wrap

                        Parts.Readout {
                            label: qsTr("Temperature")
                            value: Facade.known(gpu.modelData.temperature)
                                   ? Facade.number(gpu.modelData.temperature, 0) + "°C"
                                   : Facade.unavailable()
                            ramp: Tk.Theme.ramp.thermal
                            rampValue: Facade.known(gpu.modelData.temperature)
                                       ? gpu.modelData.temperature : 0
                            rampFrom: 30
                            rampTo: 95
                        }
                        Parts.Readout {
                            label: qsTr("Power")
                            value: Facade.known(gpu.modelData.power)
                                   ? Facade.number(gpu.modelData.power, 1) + " W"
                                   : Facade.unavailable()
                        }
                        Parts.Readout {
                            label: qsTr("Memory total")
                            value: Facade.bytes(gpu.modelData.memoryTotal)
                        }
                    }
                }
            }

            Tk.SectionHeader {
                width: parent.width
                visible: panel.sensorsOfKind("temperature").length > 0
                title: qsTr("Temperatures")
            }
            Repeater {
                model: panel.sensorsOfKind("temperature")
                delegate: Parts.MeterRow {
                    id: temp
                    required property var modelData
                    width: parent.width
                    label: temp.modelData.name
                    labelWidth: 130
                    valueWidth: 56
                    from: 20
                    to: 100
                    amount: temp.modelData.value
                    value: Facade.number(temp.modelData.value, 1) + "°C"
                    ramp: Tk.Theme.ramp.thermal
                }
            }

            Tk.SectionHeader {
                width: parent.width
                visible: panel.sensorsOfKind("fan").length > 0
                title: qsTr("Fans")
            }
            Repeater {
                model: panel.sensorsOfKind("fan")
                delegate: Tk.KeyValue {
                    required property var modelData
                    width: parent.width
                    key: modelData.name
                    value: Facade.number(modelData.value, 0) + " " + modelData.unit
                }
            }

            Tk.SectionHeader {
                width: parent.width
                visible: panel.sensorsOfKind("power").length > 0
                             || panel.sensorsOfKind("voltage").length > 0
                title: qsTr("Power")
            }
            Repeater {
                model: panel.sensorsOfKind("power").concat(panel.sensorsOfKind("voltage"))
                delegate: Tk.KeyValue {
                    required property var modelData
                    width: parent.width
                    key: modelData.name
                    value: Facade.number(modelData.value, 2) + " " + modelData.unit
                }
            }
        }
    }
}
