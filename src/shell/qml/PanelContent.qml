// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

Item {
    id: root

    required property var panel
    required property var theme
    // PanelQuickConfig facade (optional): backs the right-click panel menu
    // and the persisted per-panel quick settings in panels.configuration.
    property var panelQuickConfig: null
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
    // Worn Luna taskbar dressing follows the same derivation rule as dock
    // mode: a named fill taskbar panel opts in through its profile identity.
    // Profiles without that panel id keep the token-driven material.
    readonly property bool lunaMode: horizontal && panel.edge === "bottom"
        && panel.alignment === "fill" && panel.id === "bliss-taskbar"
    // Bumping panelConfigVersion marks the resolved-settings cache stale when
    // the facade publishes new Settings1 truth.
    property int panelConfigVersion: 0
    onPanelQuickConfigChanged: {
        if (panelQuickConfig !== null
            && panelQuickConfig.panelSettingsChanged !== undefined) {
            panelQuickConfig.panelSettingsChanged.connect(
                () => panelConfigVersion++)
            panelConfigVersion++
        }
    }
    readonly property var panelConfig: {
        void panelConfigVersion
        if (panelQuickConfig === null
            || panelQuickConfig.panelSettings === undefined) {
            return {}
        }
        return panelQuickConfig.panelSettings(String(panel.id ?? ""))
    }
    // Effective quick settings: schema defaults first, persisted values win.
    readonly property bool panelTransparency:
        panelConfig.transparency !== undefined ? panelConfig.transparency : true
    readonly property bool dockZoom:
        panelConfig.dockZoom !== undefined ? panelConfig.dockZoom : true
    readonly property int dockTileSize:
        panelConfig.dockTileSize !== undefined ? panelConfig.dockTileSize : 60
    readonly property bool reducedMotion: Tokens.ready
        && Boolean(Tokens.accessibility.reducedMotion)
    // AGENT-CONTRACT: translucency is one published truth for the whole
    // surface. The accessibility projection always wins (reduced transparency
    // and high contrast flatten to opaque), and the per-panel quick setting
    // can switch the effect off. PanelSurfaceBlur consumes the same value, so
    // the blur region can never outlive its translucent material.
    readonly property bool materialTranslucent:
        Tokens.ready && panelTransparency
        && !Boolean(Tokens.accessibility.reducedTransparency)
        && !Boolean(Tokens.accessibility.highContrast)
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
        radius: root.lunaMode ? 0
            : root.panel.alignment === "fill" ? 0
            : root.dockMode ? Tokens.radius.l : Tokens.radius.m
        // QST supplies flattened opaque backgrounds whenever accessibility
        // disables transparency or the per-panel quick setting is off. The
        // translucent material pairs with the blur-behind request driven by
        // the shell's PanelSurfaceBlur (ADR-0120): one flag, one contract.
        // The Luna taskbar paints its own weathered Luna gradient and stays
        // opaque, so the blur contract never has to track a moving region.
        color: root.lunaMode ? "transparent"
            : Tokens.ready ? (root.dockMode ? Tokens.bg.raised : Tokens.bg.base)
                           : (root.colors.surface ?? "#222624")
        gradient: root.lunaMode ? lunaMaterialGradient : null
        opacity: root.lunaMode ? 1 : (root.materialTranslucent ? 0.8 : 1)
        border.color: root.lunaMode ? "transparent"
            : Tokens.ready ? Tokens.outline.divider
                           : (root.colors.border ?? "#3c433f")
        border.width: root.lunaMode ? 0
            : root.dockMode ? 1
            : root.panel.alignment === "fill" ? 0 : 1

        Gradient {
            id: lunaMaterialGradient
            GradientStop { position: 0.0; color: "#2050b4" }
            GradientStop { position: 0.08; color: "#3b74dd" }
            GradientStop { position: 0.62; color: "#2e68d6" }
            GradientStop { position: 1.0; color: "#1b47a4" }
        }

        // Surviving gloss line along the taskbar's top seam.
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#6fa0f0"
            visible: root.lunaMode
        }

        // Right-click on the panel's own surface (behind every applet chip)
        // opens the panel configuration menu. Left clicks pass through; the
        // input mask keeps the transparent margins click-proof already.
        MouseArea {
            id: panelConfigArea
            objectName: "panelConfigArea"
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            enabled: root.panelQuickConfig !== null
            onClicked: panelConfigMenu.popup()
        }
    }

    // AGENT-NOTE: Luna notification-area well (ADR-0124, "Luna taskbar
    // rendering"): a full-height lighter band from just before the end zone
    // to the trailing edge, with a dark leading seam and a light highlight.
    // It follows the solved end-zone geometry, reserves no space, and only
    // lunaMode paints it, so every other panel renders exactly as before.
    Rectangle {
        id: lunaTrayWell
        objectName: "lunaTrayWell"
        // Luna dressing constants (ADR-0124); the seams derive from them.
        readonly property color deepBlue: "#0c59b9"
        readonly property color brightBlue: "#139ee9"
        visible: root.lunaMode && endZone.desiredExtent > 0
        x: Math.max(0, endZone.x - Tokens.space["2"])
        width: Math.max(0, root.width - x)
        height: root.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: lunaTrayWell.deepBlue }
            GradientStop { position: 0.08; color: lunaTrayWell.brightBlue }
            GradientStop { position: 1.0; color: Qt.darker(lunaTrayWell.brightBlue, 1.15) }
        }
        Rectangle { objectName: "lunaTrayWellSeam"; width: 1; height: parent.height; color: Qt.darker(lunaTrayWell.deepBlue, 1.35) }
        Rectangle { objectName: "lunaTrayWellHighlight"; x: 1; width: 1; height: parent.height; color: Qt.lighter(lunaTrayWell.brightBlue, 1.35) }
    }

    // Panel configuration menu (QQC2 style palette; Controls ships no menu
    // primitive yet — task-list context menu precedent). Every quick setting
    // persists through the PanelQuickConfig facade into Settings1.
    T.Menu {
        id: panelConfigMenu
        objectName: "panelConfigMenu"
        popupType: T.Popup.Window

        T.MenuItem {
            objectName: "panelConfigCustomize"
            text: qsTr("Customize Panel…")
            enabled: root.panelQuickConfig !== null
            onTriggered: root.panelQuickConfig.openCustomize()
        }
        T.MenuSeparator {}
        T.MenuItem {
            objectName: "panelConfigTransparency"
            text: qsTr("Transparency")
            checkable: true
            checked: root.panelTransparency
            enabled: Tokens.ready && !Boolean(Tokens.accessibility.reducedTransparency)
                     && !Boolean(Tokens.accessibility.highContrast)
            onTriggered: root.applyPanelSetting("transparency", checked)
        }
        T.MenuItem {
            objectName: "panelConfigDockZoom"
            visible: root.dockMode
            text: qsTr("Magnification")
            checkable: true
            checked: root.dockZoom
            enabled: Tokens.ready && !Boolean(Tokens.accessibility.reducedMotion)
            onTriggered: root.applyPanelSetting("dockZoom", checked)
        }
        T.Menu {
            objectName: "panelConfigTileSize"
            title: qsTr("Tile size")
            visible: root.dockMode

            T.MenuItem {
                text: qsTr("Compact (56)")
                checkable: true
                checked: root.dockTileSize === 56
                onTriggered: root.applyPanelSetting("dockTileSize", 56)
            }
            T.MenuItem {
                text: qsTr("Default (60)")
                checkable: true
                checked: root.dockTileSize === 60
                onTriggered: root.applyPanelSetting("dockTileSize", 60)
            }
            T.MenuItem {
                text: qsTr("Large (64)")
                checkable: true
                checked: root.dockTileSize === 64
                onTriggered: root.applyPanelSetting("dockTileSize", 64)
            }
        }
    }

    // AGENT-GUARD: zones receive disjoint viewport budgets. Natural content
    // may scroll within its zone, but cannot obscure another zone's controls.
    function applyPanelSetting(key, value) {
        if (panelQuickConfig !== null) {
            return panelQuickConfig.setPanelSetting(String(panel.id ?? ""),
                                                    key, value)
        }
        return false
    }

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
        lunaMode: root.lunaMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
        dockZoomEnabled: root.dockZoom
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
        lunaMode: root.lunaMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
        dockZoomEnabled: root.dockZoom
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
        lunaMode: root.lunaMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
        dockZoomEnabled: root.dockZoom
    }
}
