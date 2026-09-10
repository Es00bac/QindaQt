// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls as T
import QindaQt.Tokens 1.0

Flickable {
    id: root

    required property var panel
    required property var theme
    required property string zone
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
    property bool vertical: false
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    property bool dockZoomEnabled: true
    readonly property int lanes: Math.max(1, Number(panel.rows ?? 1))
    readonly property var zoneApplets: (panel.applets ?? []).filter(
        applet => appletZone(applet) === zone)
    readonly property real desiredExtent: vertical ? grid.implicitHeight : grid.implicitWidth
    contentWidth: vertical ? width : grid.implicitWidth
    contentHeight: vertical ? grid.implicitHeight : height
    flickableDirection: vertical ? Flickable.VerticalFlick : Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    // AGENT-GUARD: an unconditional attached scroll bar is a full-width,
    // pointer-interactive overlay across the last ~10 logical pixels of the
    // zone even while it paints at zero opacity. On a stock 30px panel that
    // is the bottom third of the 26px applet row plus its trailing edge, and
    // it consumed presses aimed at the hosted control underneath -- the
    // "clicking the lower part of a menu word does nothing" and "status icons
    // are hard to hit" defects. Each bar therefore exists only while its own
    // axis actually overflows, and stays a non-interactive indicator: the
    // zone remains scrollable by flick, wheel and keyboard reveal, which is
    // the documented overflow contract.
    T.ScrollBar.horizontal: T.ScrollBar {
        id: horizontalOverflowBar
        objectName: "panelZoneHorizontalOverflowBar"
        policy: T.ScrollBar.AsNeeded
        interactive: false
        visible: !root.vertical && root.contentWidth > root.width
        // Thin rounded token indicator: paints inside the zone's own trailing
        // padding instead of the Basic style's full-depth groove.
        implicitHeight: 4
        background: Item {}
        contentItem: Rectangle {
            implicitHeight: 4
            radius: height / 2
            color: Tokens.ready ? Tokens.fg.default
                                : (root.theme.colors ?? ({})).foreground ?? "#dddddd"
            opacity: horizontalOverflowBar.pressed ? 0.9 : 0.45
            visible: horizontalOverflowBar.size < 1
            Accessible.ignored: true
        }
    }
    T.ScrollBar.vertical: T.ScrollBar {
        id: verticalOverflowBar
        objectName: "panelZoneVerticalOverflowBar"
        policy: T.ScrollBar.AsNeeded
        interactive: false
        visible: root.vertical && root.contentHeight > root.height
        implicitWidth: 4
        background: Item {}
        contentItem: Rectangle {
            implicitWidth: 4
            radius: width / 2
            color: Tokens.ready ? Tokens.fg.default
                                : (root.theme.colors ?? ({})).foreground ?? "#dddddd"
            opacity: verticalOverflowBar.pressed ? 0.9 : 0.45
            visible: verticalOverflowBar.size < 1
            Accessible.ignored: true
        }
    }

    // Keyboard navigation must reveal the focused control inside overflow,
    // without stealing focus from applet popup windows.
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            const item = root.Window.window.activeFocusItem
            let ancestor = item
            while (ancestor && ancestor !== grid)
                ancestor = ancestor.parent
            if (!ancestor || !item)
                return
            const point = item.mapToItem(root.contentItem, 0, 0)
            if (root.vertical) {
                root.contentY = Math.max(0, Math.min(root.contentHeight - root.height,
                    Math.min(point.y, Math.max(root.contentY, point.y + item.height - root.height))))
            } else {
                root.contentX = Math.max(0, Math.min(root.contentWidth - root.width,
                    Math.min(point.x, Math.max(root.contentX, point.x + item.width - root.width))))
            }
        }
    }

    function appletZone(applet) {
        const settings = applet.settings ?? ({});
        return settings.zone ?? "start";
    }

    function isDockLauncher(applet) {
        const plugin = String(applet.plugin ?? "")
        return plugin === "launcher" || plugin === "application-launcher"
            || plugin === "quick-launch"
    }

    function dockHasVisibleLauncherBefore(applet) {
        const targetIndex = zoneApplets.indexOf(applet)
        for (let index = 0; index < targetIndex; ++index) {
            if (!isDockLauncher(zoneApplets[index]))
                continue
            const chip = repeater.itemAt(index)
            if (chip !== null && !chip.emptyLiveContent)
                return true
        }
        return false
    }

    Grid {
        id: grid
        rows: root.vertical ? -1 : root.lanes
        columns: root.vertical ? root.lanes : -1
        flow: root.vertical ? Grid.LeftToRight : Grid.TopToBottom
        spacing: 4

        Repeater {
            model: root.zoneApplets
            id: repeater

            AppletChip {
                required property var modelData

                // AGENT-GUARD: do not override AppletChip's zero extent for an
                // empty live applet; doing so resurrects an invisible panel slot.
                height: emptyLiveContent ? 0 : root.vertical ? implicitHeight
                    : Math.max(1, (root.height - (root.lanes - 1) * grid.spacing) / root.lanes)
                width: emptyLiveContent ? 0 : root.vertical
                    ? Math.max(1, (root.width - (root.lanes - 1) * grid.spacing) / root.lanes)
                    : implicitWidth
                vertical: root.vertical
                applet: modelData
                theme: root.theme
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
                dockHasLauncherGroup: root.dockMode
                    && root.dockHasVisibleLauncherBefore(modelData)
            }
        }
    }
}
