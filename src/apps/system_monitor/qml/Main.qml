// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaTK.QindaQt
import QindaQt.SystemMonitor
import "panels" as Panels
import "parts" as Parts

Tk.AppWindow {
    id: window

    // "dashboard", or one panel id when started with --panel.
    property string panel: typeof initialPanel === "string" ? initialPanel : "dashboard"
    readonly property bool single: window.panel !== "dashboard"
    property string message: ""

    // AGENT-CONTRACT: written by the AppShell menu export in main.cpp. True
    // until the desktop's global menu takes the menu over, false once it
    // hosts it. Nothing else may assign it.
    property bool inWindowMenuVisible: true

    readonly property var actionList: {
        let actions = []
        for (const menu of coordinator.menus) {
            actions = actions.concat(menu.actions)
        }
        return actions
    }

    width: window.single ? 560 : 1280
    height: window.single ? 600 : 860
    visible: true
    title: window.single
           ? qsTr("%1 — System Monitor").arg(window.panel)
           : qsTr("System Monitor")

    // Feeds the desktop's QST-1 tokens into the toolkit theme, so the monitor
    // follows whatever theme the session is wearing.
    QindaQtTheme {}

    function dispatch(actionId) {
        if (actionId === "view.pause") {
            window.setPaused(!Monitor.paused)
            return
        }
        if (actionId === "view.refresh") {
            Monitor.requestSample()
            Facade.refreshHardware()
            return
        }
        const interval = Facade.intervalForAction(actionId)
        if (interval > 0) {
            Monitor.interval = interval
            return
        }
        if (actionId === "layout.reset" && dashboardLoader.item !== null) {
            dashboardLoader.item.resetLayout()
            return
        }
        if (actionId === "layout.show-all" && dashboardLoader.item !== null) {
            dashboardLoader.item.showEveryPanel()
            return
        }
        if (actionId === "help.about") {
            about.open()
        }
    }

    function setPaused(paused) {
        Monitor.paused = paused
        Facade.paused = paused
    }

    function report(text) {
        window.message = text
        messageTimer.restart()
    }

    function detach(id) {
        // ADR-0108: a detached view is this executable again with one panel,
        // which keeps every window independent -- closing one leaves the rest
        // sampling.
        if (!Facade.openPanel(id)) {
            window.report(qsTr("Could not open a separate %1 window").arg(id))
        }
    }

    Connections {
        target: coordinator
        function onActionRequested(actionId) { window.dispatch(actionId) }
    }

    // What the menus show as checked is published from C++ (see main.cpp);
    // both copies of the menu read that one snapshot.
    Connections {
        target: Monitor
        function onErrorOccurred(message) { window.report(message) }
    }

    // Shortcuts come from the catalog too, so they work whichever place the
    // menu is being drawn -- including when it is drawn nowhere in this window.
    Instantiator {
        model: window.actionList
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.shortcut
            enabled: modelData.enabled
            onActivated: coordinator.activateAction(modelData.id)
        }
    }

    menuBar: Parts.MonitorMenuBar {
        objectName: "monitorMenuBar"
        visible: window.inWindowMenuVisible
        menusModel: coordinator.menus
        onActivated: actionId => coordinator.activateAction(actionId)
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

    // A failed signal is the answer to something the reader just tried, so it
    // belongs in the status bar for long enough to read and no longer.
    Timer {
        id: messageTimer
        interval: 6000
        onTriggered: window.message = ""
    }

    Tk.MessageDialog {
        id: about
        title: qsTr("System Monitor")
        text: qsTr("QindaQt System Monitor\n\nProcessor, memory, storage, network and "
                   + "hardware activity, and the processes behind them.")
    }
}
