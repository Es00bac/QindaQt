// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// One labelled bar: name, meter, figure. The row btop draws for a core, a
// filesystem or a sensor, with the figure taking the meter's own colour so
// the two never disagree.
Tk.Flex {
    id: row

    property string label: ""
    property string value: ""
    property real amount: 0
    property real from: 0
    property real to: 100
    property var ramp: null
    property int segments: 0
    property real labelWidth: 56
    property real valueWidth: 64
    property alias trailing: trailingHost.data

    align: Tk.Flex.Center
    gap: Tk.Theme.space.sm
    implicitHeight: Tk.Theme.size.row

    Tk.Mono {
        text: row.label
        width: row.labelWidth
        elide: Text.ElideRight
        color: Tk.Theme.color.textMuted
        Tk.Flex.shrink: 0
    }
    Tk.Meter {
        id: bar
        Tk.Flex.grow: 1
        Tk.Flex.minWidth: 24
        implicitHeight: Tk.Theme.size.meter
        value: row.amount
        from: row.from
        to: row.to
        ramp: row.ramp
        segments: row.segments
        radius: Tk.Theme.radius.xs
    }
    Tk.Mono {
        text: row.value
        width: row.valueWidth
        horizontalAlignment: Text.AlignRight
        color: row.ramp !== null ? bar.valueColor : Tk.Theme.color.text
        Tk.Flex.shrink: 0
    }
    Tk.Flex {
        id: trailingHost
        align: Tk.Flex.Center
        gap: Tk.Theme.space.xs
        visible: children.length > 0
        Tk.Flex.shrink: 0
    }
}
