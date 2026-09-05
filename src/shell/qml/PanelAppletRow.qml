// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls as T

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
    property bool vertical: false
    readonly property int lanes: Math.max(1, Number(panel.rows ?? 1))
    readonly property var zoneApplets: (panel.applets ?? []).filter(
        applet => appletZone(applet) === zone)
    readonly property real desiredExtent: vertical ? grid.implicitHeight : grid.implicitWidth
    contentWidth: vertical ? width : grid.implicitWidth
    contentHeight: vertical ? grid.implicitHeight : height
    flickableDirection: vertical ? Flickable.VerticalFlick : Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    T.ScrollBar.horizontal: T.ScrollBar { }
    T.ScrollBar.vertical: T.ScrollBar { }

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

    Grid {
        id: grid
        rows: root.vertical ? -1 : root.lanes
        columns: root.vertical ? root.lanes : -1
        flow: root.vertical ? Grid.LeftToRight : Grid.TopToBottom
        spacing: 4

        Repeater {
            model: root.zoneApplets

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
            }
        }
    }
}
