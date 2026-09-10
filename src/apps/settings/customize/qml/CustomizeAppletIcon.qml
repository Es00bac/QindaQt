// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Plugin-applet glyph for the Customize preview: one confined icon element
// with the route's static plugin mapping and the typed letter fallback when
// a glyph cannot resolve. Every applet visual in this route renders through
// this component so the preview, palette, and inspector can never disagree,
// and the mapping stays with the only element that consumes it.
Item {
    id: root

    property string pluginId: ""
    property int iconSize: 18
    // AGENT-GUARD: bundled symbolic SVGs stroke the theme canvas color and
    // are invisible unless recolored. "No recolor" is represented by a fully
    // transparent color, so the default must stay an opaque token; every
    // override must be opaque too or the glyph disappears on dark themes.
    property color glyphColor: Tokens.fg.default

    implicitWidth: root.iconSize
    implicitHeight: root.iconSize

    // Mirrors the presentation mapping the desktop panel chips use. It is a
    // static projection: runtime-dependent glyphs (live network strength)
    // collapse to a representative icon because a preview never claims live
    // state.
    function iconName(pluginId): string {
        const plugin = String(pluginId ?? "")
        if (["launcher", "application-launcher", "system-menu", "classic-menu",
             "application-menu", "start-menu", "quick-launch"].includes(plugin))
            return "start-here-kde"
        if (plugin === "system-status") return "network-wireless-signal-good"
        if (plugin === "bluetooth") return "network-bluetooth-inactive-symbolic"
        if (plugin === "power") return "battery-missing"
        if (plugin === "audio") return "audio-volume-muted"
        if (plugin === "clipboard") return "edit-paste"
        if (plugin === "notification-center") return "notifications"
        if (plugin === "status-notifier") return "preferences-plugin"
        if (["task-list", "dock-task-list", "grouped-task-list",
             "centered-task-list"].includes(plugin))
            return "preferences-system-windows"
        if (["workspace-switcher", "workspace-pager", "workspace-tiles",
             "overview-trigger", "overview"].includes(plugin))
            return "view-grid"
        if (plugin === "show-desktop") return "user-desktop"
        if (plugin === "dashboard") return "dashboard-show"
        if (["command-palette", "command-hud", "hud"].includes(plugin))
            return "system-search"
        if (plugin === "active-application") return "application-x-executable"
        if (plugin === "clock") return "clock"
        return "applications-other"
    }

    function displayLabel(pluginId): string {
        const plugin = String(pluginId ?? "")
        const labels = {
            "dock-task-list": qsTr("Task list"),
            "grouped-task-list": qsTr("Task list"),
            "centered-task-list": qsTr("Task list"),
            "global-menu": qsTr("Application menu"),
            "system-menu": qsTr("System menu"),
            "system-status": qsTr("Network status"),
            "clock": qsTr("Clock"),
            "notification-center": qsTr("Notifications"),
            "bluetooth": qsTr("Bluetooth"),
            "power": qsTr("Power"),
            "audio": qsTr("Audio"),
            "launcher": qsTr("Applications"),
            "application-launcher": qsTr("Applications"),
            "quick-launch": qsTr("Quick launch"),
            "workspace-switcher": qsTr("Workspaces"),
            "workspace-pager": qsTr("Workspaces"),
            "workspace-tiles": qsTr("Workspace tiles"),
            "overview-trigger": qsTr("Overview"),
            "dashboard": qsTr("Dashboard"),
            "show-desktop": qsTr("Show desktop"),
            "command-palette": qsTr("Command palette"),
            "command-hud": qsTr("Command HUD"),
            "active-application": qsTr("Active application"),
            "application-tiles": qsTr("Application tiles"),
            "places-menu": qsTr("Places"),
            "system-tray": qsTr("Status tray")
        }
        return labels[plugin] ?? plugin.replace(/-/g, " ")
    }

    ShellIcons.Icon {
        anchors.fill: parent
        name: root.iconName(root.pluginId)
        size: root.iconSize
        symbolic: true
        color: root.glyphColor
        fallbackText: root.displayLabel(root.pluginId)
    }
}
