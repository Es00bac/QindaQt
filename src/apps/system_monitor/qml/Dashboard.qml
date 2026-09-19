// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "panels" as Panels

// The dense arrangement: everything at once, which is the whole reason to
// prefer this over a tabbed monitor, on Tk.DockHost so the reader can move
// it. Processes take the centre because that is where a reading turns into
// a decision; the processor spans the top because its trace is what people
// scan first.
//
// Drag a panel header to re-dock or tear it off, drag a seam to resize,
// and the arrangement persists under `storageKey` -- so the layout someone
// settles on is the one they get back tomorrow.
Item {
    id: dashboard

    signal detachRequested(string panelId)
    signal actionFailed(string message)

    function resetLayout() { dock.resetLayout() }
    function showEveryPanel() {
        for (const id of dock.hiddenPanels) {
            dock.showPanel(id)
        }
    }
    readonly property var hiddenPanels: dock.hiddenPanels

    Tk.DockHost {
        id: dock
        objectName: "monitorDock"
        anchors.fill: parent
        workspace: "monitor"

        model: Tk.DockModel { storageKey: "qindaqt/system-monitor/layout" }

        // AGENT-NOTE: the canvas is the process table, not an empty surface.
        // A dock host's canvas is the one region that cannot be torn off or
        // hidden, and the process list is the panel this application would be
        // pointless without.
        canvas: Panels.ProcessPanel {
            framed: false
            onDetachRequested: function(id) { dashboard.detachRequested(id) }
            onActionFailed: function(message) { dashboard.actionFailed(message) }
        }

        Tk.DockPanel {
            panelId: "cpu"
            title: qsTr("Processor")
            iconName: "cpu"
            zone: "top"
            extent: 300
            minHeight: 140
            chrome: "compact"
            padding: 0
            Panels.CpuPanel {
                framed: false
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
        }
        Tk.DockPanel {
            panelId: "memory"
            title: qsTr("Memory")
            iconName: "memory-stick"
            zone: "left"
            order: 0
            extent: 300
            minWidth: 220
            chrome: "compact"
            padding: 0
            Panels.MemoryPanel {
                framed: false
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
        }
        Tk.DockPanel {
            panelId: "disks"
            title: qsTr("Storage")
            iconName: "hard-drive"
            zone: "left"
            order: 1
            extent: 300
            minWidth: 220
            chrome: "compact"
            padding: 0
            Panels.DisksPanel {
                framed: false
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
        }
        Tk.DockPanel {
            panelId: "network"
            title: qsTr("Network")
            iconName: "network"
            zone: "bottom"
            order: 0
            extent: 190
            minHeight: 120
            chrome: "compact"
            padding: 0
            Panels.NetworkPanel {
                framed: false
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
        }
        Tk.DockPanel {
            panelId: "hardware"
            title: qsTr("Hardware")
            iconName: "thermometer"
            zone: "right"
            order: 0
            extent: 290
            minWidth: 220
            chrome: "compact"
            padding: 0
            Panels.HardwarePanel {
                framed: false
                onDetachRequested: function(id) { dashboard.detachRequested(id) }
            }
        }
    }
}
