// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaTK.QindaQt
import QindaQt.Link
import "pages" as Pages
import "parts" as Parts

// QindaLink: a rail of pages beside the active page, dense and dark like the System Monitor.
// The pages talk to the view-model singletons (Devices, Threads, Conversation, PhoneApps, Mutes,
// Launcher); this file owns only navigation, the menu and the status bar.
Tk.AppWindow {
    id: window

    property string page: typeof initialPage === "string" ? initialPage : "devices"
    property string message: ""

    // AGENT-CONTRACT: written by the AppShell menu export in main.cpp. True until the desktop's
    // global menu takes the menu over, false once it hosts it. Nothing else may assign it.
    property bool inWindowMenuVisible: true

    readonly property var actionList: {
        let actions = []
        for (const menu of coordinator.menus)
            actions = actions.concat(menu.actions)
        return actions
    }

    width: 1100
    height: 720
    visible: true
    title: qsTr("QindaLink")

    QindaQtTheme {}

    function go(id) {
        window.page = id
        rail.confirmRoute(id)
    }

    function dispatch(actionId) {
        if (actionId === "view.refresh") {
            Devices.refresh()
            Threads.reload()
            Mutes.refresh()
            if (window.page === "apps")
                PhoneApps.refresh()
            return
        }
        if (actionId === "help.about") {
            about.open()
            return
        }
        if (actionId.indexOf("go.") === 0)
            window.go(actionId.substring(3))
    }

    function report(text) {
        window.message = text
        messageTimer.restart()
    }

    Connections {
        target: coordinator
        function onActionRequested(actionId) { window.dispatch(actionId) }
    }

    // Shortcuts come from the catalog too, so they work whichever place the menu is drawn --
    // including when it is drawn nowhere in this window.
    Instantiator {
        model: window.actionList
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.shortcut
            enabled: modelData.enabled
            onActivated: coordinator.activateAction(modelData.id)
        }
    }

    menuBar: Parts.LinkMenuBar {
        objectName: "linkMenuBar"
        visible: window.inWindowMenuVisible
        menusModel: coordinator.menus
        onActivated: actionId => coordinator.activateAction(actionId)
    }

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Row

        Tk.Box {
            Tk.Flex.shrink: 0
            implicitWidth: 176
            color: Tk.Theme.color.panel
            borderRight: 1
            borderColor: Tk.Theme.color.divider

            Tk.NavigationRail {
                id: rail
                objectName: "linkRail"
                anchors.fill: parent
                tooltip: qsTr("QindaLink pages")
                currentId: window.page
                groups: [{
                    id: "pages",
                    label: qsTr("Phone"),
                    entries: [
                        { id: "devices", label: qsTr("Devices"), iconName: "smartphone",
                          badge: Devices.pairings.count > 0 ? String(Devices.pairings.count) : "" },
                        { id: "messages", label: qsTr("Messages"), iconName: "message-square",
                          badge: Threads.unread > 0 ? String(Threads.unread) : "" },
                        { id: "apps", label: qsTr("Phone apps"), iconName: "layout-grid" },
                        { id: "notifications", label: qsTr("Notifications"), iconName: "bell" },
                        { id: "input", label: qsTr("Input & macros"), iconName: "keyboard" },
                        { id: "vault", label: qsTr("Vault"), iconName: "lock" }
                    ]
                }]
                onDestinationRequested: (id, destination) => window.go(id)
            }
        }

        Loader {
            id: pageLoader
            objectName: "linkPage"
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Flex.minWidth: 0
            sourceComponent: window.page === "messages" ? messagesPage
                           : window.page === "apps" ? appsPage
                           : window.page === "notifications" ? notificationsPage
                           : window.page === "input" ? inputPage
                           : window.page === "vault" ? vaultPage : devicesPage
        }
    }

    Component { id: devicesPage; Pages.DevicesPage {} }
    Component { id: messagesPage; Pages.MessagesPage {} }
    Component { id: appsPage; Pages.PhoneAppsPage {} }
    Component { id: notificationsPage; Pages.NotificationsPage {} }
    Component { id: inputPage; Pages.InputPage {} }
    Component { id: vaultPage; Pages.VaultPage {} }

    statusBar: Tk.StatusBar {
        Tk.StatusField {
            iconName: Devices.serviceAvailable ? "link" : "unlink"
            text: Devices.serviceAvailable ? qsTr("Service running") : qsTr("Service not running")
        }
        Tk.StatusField {
            iconName: "smartphone"
            text: !Devices.hasDevice ? qsTr("No phone")
                  : Devices.phoneReady ? Devices.device.name
                  : qsTr("%1 (not connected)").arg(Devices.device.name)
        }
        Tk.StatusField {
            visible: Devices.hasDevice && Devices.device.battery.length > 0
            text: qsTr("Battery %1").arg(Devices.device.battery)
        }
        Tk.Spacer {}
        Tk.StatusField {
            objectName: "statusMessage"
            iconName: window.message.length > 0 ? "triangle-alert" : ""
            text: window.message
        }
    }

    Timer {
        id: messageTimer
        interval: 6000
        onTriggered: window.message = ""
    }

    Tk.MessageDialog {
        id: about
        title: qsTr("QindaLink")
        text: qsTr("QindaLink\n\nYour phone on the desktop: messages, notifications, phone apps and input "
                   + "over USB. Pair once; nothing leaves your machine.")
    }
}
