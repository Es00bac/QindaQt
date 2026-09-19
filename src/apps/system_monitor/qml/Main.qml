// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC
import QindaTK as Tk
import QindaTK.QindaQt
import QindaQt.SystemMonitor
import "panels" as Panels

Tk.AppWindow {
    id: window

    // "dashboard", or one panel id when started with --panel.
    property string panel: typeof initialPanel === "string" ? initialPanel : "dashboard"
    readonly property bool single: window.panel !== "dashboard"

    width: window.single ? 560 : 1280
    height: window.single ? 600 : 860
    visible: true
    title: window.single
           ? qsTr("%1 — System Monitor").arg(window.panel)
           : qsTr("System Monitor")

    // Feeds the desktop's QST-1 tokens into the toolkit theme, so the monitor
    // follows whatever theme the session is wearing.
    QindaQtTheme {}

    menuBar: Tk.MenuBar {
        Tk.Menu {
            title: qsTr("&View")
            Tk.MenuItem {
                text: qsTr("Pause updates")
                shortcut: "Ctrl+Alt+P"
                checkable: true
                checked: Monitor.paused
                onTriggered: {
                    Monitor.paused = checked
                    Facade.paused = checked
                }
            }
            Tk.MenuItem {
                text: qsTr("Refresh now")
                shortcut: "F5"
                onTriggered: { Monitor.requestSample(); Facade.refreshHardware() }
            }
            Tk.MenuSeparator {}
            Tk.MenuItem {
                text: qsTr("Fast (500 ms)")
                radio: true
                checked: Monitor.interval === 500
                onTriggered: Monitor.interval = 500
            }
            Tk.MenuItem {
                text: qsTr("Normal (1 s)")
                radio: true
                checked: Monitor.interval === 1000
                onTriggered: Monitor.interval = 1000
            }
            Tk.MenuItem {
                text: qsTr("Relaxed (3 s)")
                radio: true
                checked: Monitor.interval === 3000
                onTriggered: Monitor.interval = 3000
            }
        }
        Tk.Menu {
            title: qsTr("&Layout")
            enabled: !window.single
            Tk.MenuItem {
                text: qsTr("Reset arrangement")
                onTriggered: if (dashboardLoader.item !== null) dashboardLoader.item.resetLayout()
            }
            Tk.MenuItem {
                text: qsTr("Show every panel")
                onTriggered: if (dashboardLoader.item !== null) dashboardLoader.item.showEveryPanel()
            }
        }
        Tk.Menu {
            title: qsTr("&Help")
            Tk.MenuItem {
                text: qsTr("About System Monitor")
                onTriggered: about.open()
            }
        }
    }

    Loader {
        id: dashboardLoader
        anchors.fill: parent
        sourceComponent: window.single ? singlePanel : dashboardView
    }

    Component {
        id: dashboardView
        Dashboard {
            onDetachRequested: function(id) { window.detach(id) }
            onActionFailed: function(message) { window.report(message) }
        }
    }

    Component {
        id: singlePanel
        Item {
            Loader {
                anchors.fill: parent
                anchors.margins: Tk.Theme.space.sm
                sourceComponent: window.panel === "cpu" ? cpuOnly
                               : window.panel === "memory" ? memoryOnly
                               : window.panel === "disks" ? disksOnly
                               : window.panel === "network" ? networkOnly
                               : window.panel === "hardware" ? hardwareOnly : processesOnly
            }
        }
    }

    Component { id: cpuOnly; Panels.CpuPanel { detachable: false } }
    Component { id: memoryOnly; Panels.MemoryPanel { detachable: false } }
    Component { id: disksOnly; Panels.DisksPanel { detachable: false } }
    Component { id: networkOnly; Panels.NetworkPanel { detachable: false } }
    Component { id: hardwareOnly; Panels.HardwarePanel { detachable: false } }
    Component {
        id: processesOnly
        Panels.ProcessPanel {
            detachable: false
            onActionFailed: function(message) { window.report(message) }
        }
    }

    statusBar: Tk.StatusBar {
        Tk.StatusField {
            iconName: Monitor.paused ? "pause" : "activity"
            text: Monitor.paused ? qsTr("Paused")
                                 : qsTr("Every %1 ms").arg(Monitor.interval)
        }
        Tk.StatusField {
            text: qsTr("%1 processes").arg(Processes.totalCount)
        }
        Tk.Spacer {}
        Tk.StatusField {
            objectName: "statusMessage"
            iconName: window.message.length > 0 ? "triangle-alert" : ""
            text: window.message
        }
    }

    property string message: ""

    function report(text) {
        window.message = text
        messageTimer.restart()
    }

    // A failed signal is the user's answer to something they just tried, so
    // it belongs in the status bar for long enough to read and no longer.
    Timer {
        id: messageTimer
        interval: 6000
        onTriggered: window.message = ""
    }

    function detach(id) {
        // ADR-0108: a detached view is this executable again with one panel,
        // which keeps every window independent -- closing one leaves the rest
        // sampling.
        if (!Facade.openPanel(id)) {
            window.report(qsTr("Could not open a separate %1 window").arg(id))
        }
    }

    Tk.MessageDialog {
        id: about
        title: qsTr("System Monitor")
        text: qsTr("QindaQt System Monitor\n\nProcessor, memory, storage, network and "
                   + "hardware activity, and the processes behind them.")
    }

    Connections {
        target: Monitor
        function onErrorOccurred(message) { window.report(message) }
    }
}
