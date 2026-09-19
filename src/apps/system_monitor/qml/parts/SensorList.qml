// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaQt.SystemMonitor

// A titled run of hwmon readings of one kind. Temperatures get a bar against
// a plausible ceiling; the rest are figures, because a fan speed has no
// maximum this application can honestly claim to know.
Tk.Flex {
    id: list

    property string title: ""
    property var sensors: []
    property bool metered: false
    property real from: 20
    property real to: 100
    property var ramp: null
    property int decimals: 1

    direction: Tk.Flex.Column
    gap: 0
    visible: list.sensors.length > 0

    Tk.SectionHeader { title: list.title }

    Repeater {
        model: list.sensors
        delegate: MeterRow {
            required property var modelData
            visible: list.metered
            label: modelData.name
            labelWidth: 130
            valueWidth: 56
            from: list.from
            to: list.to
            amount: modelData.value
            value: Facade.number(modelData.value, list.decimals) + "°C"
            ramp: list.ramp
        }
    }
    Repeater {
        model: list.sensors
        delegate: Tk.KeyValue {
            required property var modelData
            visible: !list.metered
            key: modelData.name
            value: Facade.number(modelData.value, list.decimals) + " " + modelData.unit
        }
    }
}
