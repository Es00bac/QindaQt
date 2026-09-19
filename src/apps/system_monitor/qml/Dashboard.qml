// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "panels" as Panels

// The dense arrangement: everything at once, which is the whole reason to
// prefer this over a tabbed monitor. Processor spans the top because its
// trace is the one people scan first; processes take the tallest column
// because that is where the reading turns into a decision.
//
// The grid reflows rather than scrolling: below 1100px the three columns
// become two, and below 760 one, so a narrow window loses arrangement but
// never loses a panel.
Item {
    id: dashboard

    property int columns: dashboard.width >= 1100 ? 3 : dashboard.width >= 760 ? 2 : 1

    signal detachRequested(string panelId)

    Tk.Flex {
        anchors.fill: parent
        anchors.margins: Tk.Theme.space.sm
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Panels.CpuPanel {
            Tk.Flex.grow: 2
            Tk.Flex.basis: 0
            Tk.Flex.minHeight: 150
            onDetachRequested: function(id) { dashboard.detachRequested(id) }
        }

        Tk.Grid {
            Tk.Flex.grow: 5
            Tk.Flex.basis: 0
            columns: dashboard.columns === 3 ? "1fr 1fr 1.4fr"
                   : dashboard.columns === 2 ? "1fr 1fr" : "1fr"
            columnGap: Tk.Theme.space.sm
            rowGap: Tk.Theme.space.sm

            Panels.MemoryPanel {
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
            Panels.DisksPanel {
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
            Panels.ProcessPanel {
                // The process table earns two rows whenever the layout has
                // them: a ten-row table is a list, a thirty-row one is a tool.
                Tk.Grid.rowSpan: dashboard.columns === 1 ? 1 : 2
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
                onActionFailed: function(message) { dashboard.actionFailed(message) }
            }
            Panels.NetworkPanel {
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
            Panels.HardwarePanel {
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
        }
    }

    signal actionFailed(string message)
}
