// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Window {
    id: root

    required property var panel
    required property var theme
    required property string surfaceId
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

    visible: false
    color: "transparent"
    title: qsTr("QindaQt panel %1").arg(surfaceId)
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus

    PanelContent {
        anchors.fill: parent
        panel: root.panel
        theme: root.theme
        liveApplets: true
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
