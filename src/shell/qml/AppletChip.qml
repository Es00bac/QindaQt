// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

Rectangle {
    id: root
    objectName: "appletChip"

    required property var applet
    required property var theme
    property bool vertical: false
    property bool liveApplets: false
    property var notificationCenterAppletAccess: null
    property var audioAppletAccess: null
    property var bluetoothAppletAccess: null
    property var powerAppletAccess: null
    property var clipboardAppletAccess: null
    property var launcherAppletAccess: null
    property var globalMenuAppletAccess: null
    property var taskListAppletAccess: null
    property var statusNotifierAppletAccess: null
    property var desktopControlsAccess: null
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    property bool dockZoomEnabled: true
    property bool dockHasLauncherGroup: false
    readonly property var colors: theme.colors ?? ({})
    readonly property var settings: applet.settings ?? ({})
    readonly property var runtime: applet.runtime ?? ({})
    readonly property string pluginId: String(applet.plugin)
    readonly property bool usesLiveContent: builtinContent.hasLiveContent
    readonly property bool runtimeUnavailable: liveApplets && runtime.ready !== true
    readonly property bool unavailableGlobalMenu:
        pluginId === "global-menu"
        && (globalMenuAppletAccess === null
            || !Boolean(globalMenuAppletAccess.available))
    readonly property bool emptyLiveContent:
        usesLiveContent && builtinContent.implicitWidth <= 0
        && (pluginId !== "global-menu" || unavailableGlobalMenu)
    readonly property int compactExtent: Tokens.space["6"] + Tokens.space["2"]
    readonly property int minimumLiveWidth:
        pluginId === "global-menu" ? compactExtent * 2 : compactExtent

    implicitWidth: emptyLiveContent ? 0
         : vertical ? compactExtent
         : Math.max(minimumLiveWidth,
                    usesLiveContent ? builtinContent.implicitWidth : compactExtent)
    implicitHeight: emptyLiveContent ? 0 : vertical
          ? Math.max(compactExtent,
                     usesLiveContent ? builtinContent.implicitHeight : compactExtent)
          : compactExtent
    radius: dockMode ? Tokens.radius.m : Math.min(theme.cornerRadius ?? 8, 8)
    // AGENT-GUARD: live applets size themselves against the panel row. A
    // second clip here cuts their token padding on the stock 26 px panel;
    // PanelContent remains the sole surface-extent clip authority.
    clip: !usesLiveContent
    color: builtinContent.selected || hoverHandler.hovered
          ? (Tokens.ready ? Tokens.state.hover : colors.accent ?? "#8fc8b7")
          : settings.bare ? "transparent"
          : (Tokens.ready ? Tokens.bg.raised : colors.surfaceRaised ?? "#2c312e")

    function displayLabel(plugin) {
        const labels = {
            "dock-task-list": qsTr("Task list"),
            "grouped-task-list": qsTr("Task list"),
            "global-menu": qsTr("Application menu"),
            "system-menu": qsTr("System menu"),
            "system-status": qsTr("Network status"),
            "clock": qsTr("Clock"),
            "notification-center": qsTr("Notifications"),
            "bluetooth": "Bluetooth",
            "power": "Power",
            "audio": "Audio",
            "launcher": qsTr("Applications"),
            "application-launcher": qsTr("Applications")
        };
        return labels[plugin] ?? plugin.replace(/-/g, " ");
    }

    function iconName(plugin) {
        if (["launcher", "application-launcher", "system-menu", "classic-menu",
             "application-menu", "start-menu", "quick-launch"].includes(plugin))
            return "start-here-kde"
        if (plugin === "system-status") return networkIconName()
        if (plugin === "bluetooth") return "network-bluetooth-inactive-symbolic"
        if (plugin === "power") return "battery-missing"
        if (plugin === "audio") return "audio-volume-muted"
        if (plugin === "clipboard") return "edit-paste"
        if (plugin === "notification-center") return "notifications"
        if (plugin === "status-notifier") return "preferences-plugin"
        if (["dock-task-list", "grouped-task-list", "centered-task-list"].includes(plugin))
            return "preferences-system-windows"
        if (plugin === "workspace-pager") return "virtual-desktops"
        if (plugin === "show-desktop") return "user-desktop"
        if (["overview", "dashboard"].includes(plugin)) return "view-grid"
        if (["command-palette", "hud"].includes(plugin)) return "system-search"
        if (plugin === "active-application") return "application-x-executable"
        if (plugin === "clock-tile" || plugin === "clock") return "clock"
        return "applications-other"
    }

    function networkIconName() {
        if (root.liveApplets && root.runtime.ready !== true)
            return "network-wireless-off"
        const state = String(root.runtime.networkState
                             ?? root.settings.networkState ?? "disconnected")
        if (state === "unavailable") return "network-wireless-off"
        if (state !== "connected") return "network-wireless-disconnected"
        const strength = Number(root.runtime.strength
                                ?? root.settings.strength ?? 0)
        if (strength >= 75) return "network-wireless-signal-excellent"
        if (strength >= 50) return "network-wireless-signal-good"
        if (strength >= 25) return "network-wireless-signal-ok"
        return "network-wireless-signal-weak"
    }

    ShellIcons.Icon {
        id: fallbackIcon
        objectName: "staticAppletIcon"
        anchors.centerIn: parent
        name: root.iconName(root.pluginId)
        size: Math.min(20, root.height - Tokens.space["2"])
        color: Tokens.fg.default
        symbolic: true
        fallbackText: root.displayLabel(root.pluginId)
        visible: !root.usesLiveContent
    }

    BuiltinAppletContent {
        id: builtinContent
        anchors.fill: parent
        visible: root.usesLiveContent
        applet: root.applet
        theme: root.theme
        vertical: root.vertical
        liveApplets: root.liveApplets
        notificationCenterAppletAccess: root.notificationCenterAppletAccess
        audioAppletAccess: root.audioAppletAccess
        bluetoothAppletAccess: root.bluetoothAppletAccess
        powerAppletAccess: root.powerAppletAccess
        launcherAppletAccess: root.launcherAppletAccess
        globalMenuAppletAccess: root.globalMenuAppletAccess
        clipboardAppletAccess: root.clipboardAppletAccess
        taskListAppletAccess: root.taskListAppletAccess
        statusNotifierAppletAccess: root.statusNotifierAppletAccess
        desktopControlsAccess: root.desktopControlsAccess
        dockMode: root.dockMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
        dockZoomEnabled: root.dockZoomEnabled
        dockHasLauncherGroup: root.dockHasLauncherGroup
    }

    Rectangle {
        objectName: "appletUnavailableMarker"
        // AGENT-NOTE: an unavailable plug-in stays visible as profile content,
        // but the amber marker prevents a static mock from claiming to be live.
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 3
        width: 6
        height: 6
        radius: 3
        visible: root.runtimeUnavailable
        color: root.colors.warning ?? "#e5a84b"
        border.color: root.colors.surface ?? "#222624"
        border.width: 1
    }

    HoverHandler {
        id: hoverHandler
    }
}
