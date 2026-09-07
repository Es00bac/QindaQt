// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0

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
    // AGENT-CONTRACT: dock appearance is derived from an existing solved
    // profile role. It is transient presentation state, never profile schema.
    readonly property bool dockMode: horizontal && panel.edge === "bottom"
        && panel.alignment === "center"
        && ((panel.applets ?? []).some(applet => {
                const settings = applet.settings ?? ({});
                return settings.dockMode === true;
            }) || panel.id === "dock" || panel.id === "smart-shelf")
    readonly property int dockTileSize: 60
    readonly property bool reducedMotion: Tokens.ready
        && Boolean(Tokens.accessibility.reducedMotion)
    readonly property real contentInset: Tokens.space["2"]
    // AGENT-CONTRACT: the along-axis inset (contentInset) keeps zones off the
    // panel's short edges. The cross-axis inset is independent and stays
    // deliberately small: it is subtracted twice from the panel's thickness,
    // and hosted controls (menu entries, status buttons) target 24-36px hit
    // areas. Reusing contentInset here left 22px on the stock 30px panel, so
    // a hosted control was taller than its own row and centered to a negative
    // offset -- hanging above and below the row and spending hit area outside
    // it. See docs/wiki/shell/panel-surfaces.md#panel-hit-targets.
    readonly property real crossAxisInset: Tokens.space["1"]
    readonly property bool dockUsesSideZones: dockMode
        && (startZone.desiredExtent > 0 || endZone.desiredExtent > 0)
    // The surface plan remains solver-owned. This mask only prevents the
    // transparent part of a centered dock window from swallowing desktop
    // clicks, while retaining room for the dock tile hover expansion.
    readonly property rect inputBounds: dockMode
        ? Qt.rect(Math.max(0, material.x - Tokens.space["3"]),
                  Math.max(0, material.y - Tokens.space["3"]),
                  Math.min(width, material.width + Tokens.space["6"]),
                  Math.min(height, material.height + Tokens.space["6"]))
        : Qt.rect(0, 0, width, height)

    clip: true

    Rectangle {
        id: material
        objectName: "panelMaterial"
        width: root.dockMode && !root.dockUsesSideZones
            ? Math.min(parent.width, centerZone.desiredExtent + root.contentInset * 2)
            : parent.width
        height: root.dockMode ? Math.max(0, parent.height - Tokens.space["2"]) : parent.height
        anchors.centerIn: parent
        radius: root.panel.alignment === "fill" ? 0
            : root.dockMode ? Tokens.radius.l : Tokens.radius.m
        // QST supplies flattened opaque backgrounds whenever accessibility
        // disables transparency. No local blur effect is created.
        color: Tokens.ready ? (root.dockMode ? Tokens.bg.raised : Tokens.bg.base)
                            : (root.colors.surface ?? "#222624")
        opacity: root.dockMode && !root.dockUsesSideZones && Tokens.ready
                 && !Boolean(Tokens.accessibility.reducedTransparency)
                 && !Boolean(Tokens.accessibility.highContrast) ? 0.88 : 1
        border.color: Tokens.ready ? Tokens.outline.divider
                                   : (root.colors.border ?? "#3c433f")
        border.width: root.dockMode ? 1 : root.panel.alignment === "fill" ? 0 : 1
    }

    // AGENT-GUARD: zones receive disjoint viewport budgets. Natural content
    // may scroll within its zone, but cannot obscure another zone's controls.
    readonly property real extent: Math.max(0, (horizontal ? width : height) - contentInset * 2)
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
    readonly property real centerOffset: Math.max(contentInset + zoneExtent(startZone),
        Math.min(contentInset + extent - zoneExtent(endZone) - zoneExtent(centerZone),
                 (extent - zoneExtent(centerZone)) / 2 + contentInset))

    PanelAppletRow {
        id: startZone
        objectName: "panelZoneStart"
        vertical: !root.horizontal
        x: root.horizontal ? root.contentInset : root.crossAxisInset
        y: root.horizontal ? root.crossAxisInset : root.contentInset
        width: root.horizontal ? root.zoneExtent(this) : Math.max(0, parent.width - root.crossAxisInset * 2)
        height: root.horizontal ? Math.max(0, parent.height - root.crossAxisInset * 2) : root.zoneExtent(this)
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
        dockMode: root.dockMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
    }
    PanelAppletRow {
        id: centerZone
        objectName: "panelZoneCenter"
        vertical: !root.horizontal
        x: root.horizontal ? root.centerOffset : root.crossAxisInset
        y: root.horizontal ? root.crossAxisInset : root.centerOffset
        width: root.horizontal ? root.zoneExtent(this) : Math.max(0, parent.width - root.crossAxisInset * 2)
        height: root.horizontal ? Math.max(0, parent.height - root.crossAxisInset * 2) : root.zoneExtent(this)
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
        dockMode: root.dockMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
    }
    PanelAppletRow {
        id: endZone
        objectName: "panelZoneEnd"
        vertical: !root.horizontal
        x: root.horizontal ? root.contentInset + root.extent - root.zoneExtent(endZone) : root.crossAxisInset
        y: root.horizontal ? root.crossAxisInset : root.contentInset + root.extent - root.zoneExtent(endZone)
        width: root.horizontal ? root.zoneExtent(this) : Math.max(0, parent.width - root.crossAxisInset * 2)
        height: root.horizontal ? Math.max(0, parent.height - root.crossAxisInset * 2) : root.zoneExtent(this)
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
        dockMode: root.dockMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
    }
}
