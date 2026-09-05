// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root

    required property var panel
    required property var theme
    property bool liveApplets: false
    property var notificationCenterAppletAccess: null
    property var audioAppletAccess: null
    property var bluetoothAppletAccess: null
    property var clipboardAppletAccess: null
    property var powerAppletAccess: null
    property var launcherAppletAccess: null
    property var globalMenuAppletAccess: null
    property var taskListAppletAccess: null
    property var statusNotifierAppletAccess: null
    property var desktopControlsAccess: null
    readonly property bool horizontal: panel.edge === "top" || panel.edge === "bottom"
    readonly property var colors: theme.colors ?? ({})

    clip: true

    Rectangle {
        anchors.fill: parent
        radius: root.panel.alignment === "fill" ? 0 : root.theme.cornerRadius ?? 10
        color: root.colors.surface ?? "#222624"
        border.color: root.colors.border ?? "#3c433f"
        border.width: 1
    }

    // AGENT-GUARD: zones receive disjoint viewport budgets. Natural content
    // may scroll within its zone, but cannot obscure another zone's controls.
    readonly property real extent: Math.max(0, (horizontal ? width : height) - 8)
    function zoneExtent(zone) {
        const zones = [startZone, centerZone, endZone]
            .filter(item => item.desiredExtent > 0)
            .sort((a, b) => a.desiredExtent - b.desiredExtent)
        let remaining = extent
        for (let i = 0; i < zones.length; ++i) {
            const budget = Math.min(zones[i].desiredExtent,
                                    remaining / (zones.length - i))
            if (zones[i] === zone)
                return budget
            remaining -= budget
        }
        return 0
    }
    readonly property real centerOffset: Math.max(4 + zoneExtent(startZone),
        Math.min(4 + extent - zoneExtent(endZone) - zoneExtent(centerZone),
                 (extent - zoneExtent(centerZone)) / 2 + 4))

    PanelAppletRow {
        id: startZone
        objectName: "panelZoneStart"
        vertical: !root.horizontal
        x: root.horizontal ? 4 : 4
        y: root.horizontal ? 4 : 4
        width: root.horizontal ? root.zoneExtent(this) : Math.max(0, parent.width - 8)
        height: root.horizontal ? Math.max(0, parent.height - 8) : root.zoneExtent(this)
        zone: "start"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
        notificationCenterAppletAccess: root.notificationCenterAppletAccess
        audioAppletAccess: root.audioAppletAccess
        bluetoothAppletAccess: root.bluetoothAppletAccess
        clipboardAppletAccess: root.clipboardAppletAccess
        powerAppletAccess: root.powerAppletAccess
        launcherAppletAccess: root.launcherAppletAccess
        globalMenuAppletAccess: root.globalMenuAppletAccess
        taskListAppletAccess: root.taskListAppletAccess
        statusNotifierAppletAccess: root.statusNotifierAppletAccess
        desktopControlsAccess: root.desktopControlsAccess
    }
    PanelAppletRow {
        id: centerZone
        objectName: "panelZoneCenter"
        vertical: !root.horizontal
        x: root.horizontal ? root.centerOffset : 4
        y: root.horizontal ? 4 : root.centerOffset
        width: root.horizontal ? root.zoneExtent(this) : Math.max(0, parent.width - 8)
        height: root.horizontal ? Math.max(0, parent.height - 8) : root.zoneExtent(this)
        zone: "center"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
        notificationCenterAppletAccess: root.notificationCenterAppletAccess
        audioAppletAccess: root.audioAppletAccess
        bluetoothAppletAccess: root.bluetoothAppletAccess
        clipboardAppletAccess: root.clipboardAppletAccess
        powerAppletAccess: root.powerAppletAccess
        launcherAppletAccess: root.launcherAppletAccess
        globalMenuAppletAccess: root.globalMenuAppletAccess
        taskListAppletAccess: root.taskListAppletAccess
        statusNotifierAppletAccess: root.statusNotifierAppletAccess
        desktopControlsAccess: root.desktopControlsAccess
    }
    PanelAppletRow {
        id: endZone
        objectName: "panelZoneEnd"
        vertical: !root.horizontal
        x: root.horizontal ? 4 + root.extent - root.zoneExtent(endZone) : 4
        y: root.horizontal ? 4 : 4 + root.extent - root.zoneExtent(endZone)
        width: root.horizontal ? root.zoneExtent(this) : Math.max(0, parent.width - 8)
        height: root.horizontal ? Math.max(0, parent.height - 8) : root.zoneExtent(this)
        zone: "end"
        panel: root.panel
        theme: root.theme
        liveApplets: root.liveApplets
        notificationCenterAppletAccess: root.notificationCenterAppletAccess
        audioAppletAccess: root.audioAppletAccess
        bluetoothAppletAccess: root.bluetoothAppletAccess
        clipboardAppletAccess: root.clipboardAppletAccess
        powerAppletAccess: root.powerAppletAccess
        launcherAppletAccess: root.launcherAppletAccess
        globalMenuAppletAccess: root.globalMenuAppletAccess
        taskListAppletAccess: root.taskListAppletAccess
        statusNotifierAppletAccess: root.statusNotifierAppletAccess
        desktopControlsAccess: root.desktopControlsAccess
    }
}
