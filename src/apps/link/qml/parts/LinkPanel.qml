// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The frame every page section wears: a Tk.Panel with a compact header whose controls sit at the
// right, like the System Monitor's panels. `summary` is a short reading beside the title.
Tk.Panel {
    id: panel

    property string summary: ""
    property alias controls: controlHost.data

    collapsible: false
    grip: false

    actions: Tk.Flex {
        id: header
        align: Tk.Flex.Center
        gap: Tk.Theme.space.xs

        Tk.Mono {
            visible: panel.summary.length > 0
            text: panel.summary
            color: Tk.Theme.color.textMuted
            Tk.Flex.shrink: 1
            Tk.Flex.minWidth: 0
        }
        Tk.Flex {
            id: controlHost
            align: Tk.Flex.Center
            gap: Tk.Theme.space.xs
            visible: controlHost.children.length > 0
        }
    }
}
