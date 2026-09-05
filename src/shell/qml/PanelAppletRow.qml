// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

Row {
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
    spacing: 4

    function appletZone(applet) {
        const settings = applet.settings ?? ({});
        return settings.zone ?? "start";
    }

    Repeater {
        model: root.panel.applets ?? []

        AppletChip {
            required property var modelData

            visible: root.appletZone(modelData) === root.zone
            height: root.height
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
