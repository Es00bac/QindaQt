// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor
import "../parts" as Parts

// Graphics and sensors, from whatever the installed driver exposes.
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
                delegate: Parts.GpuCard {
                    required property var modelData
                    gpu: modelData
                }
            }

            Parts.SensorList {
                title: qsTr("Temperatures")
                sensors: panel.sensorsOfKind("temperature")
                metered: true
                ramp: Tk.Theme.ramp.thermal
            }
            Parts.SensorList {
                title: qsTr("Fans")
                sensors: panel.sensorsOfKind("fan")
                decimals: 0
            }
            Parts.SensorList {
                title: qsTr("Power")
                sensors: panel.sensorsOfKind("power").concat(panel.sensorsOfKind("voltage"))
                decimals: 2
            }
        }
    }
}
