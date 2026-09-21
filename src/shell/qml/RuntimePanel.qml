// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Window {
    id: root

    required property var panel
    required property var theme
    required property string surfaceId
    property var panelQuickConfig: null
    // LiveCustomizationController facade (Meta+right-click menus, edit mode).
    property var liveCustomization: null
    property var notificationCenterAppletAccess: null
    property var audioAppletAccess: null
    property var bluetoothAppletAccess: null
    property var smartLightsAppletAccess: null
    property var voiceAppletAccess: null
    property var obsAppletAccess: null
    property var gatherOverviewAccess: null
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
        objectName: "runtimePanelContent"
        anchors.fill: parent
        panel: root.panel
        theme: root.theme
        panelQuickConfig: root.panelQuickConfig
        liveCustomization: root.liveCustomization
        outputId: root.surfaceId.indexOf("@") >= 0 ? root.surfaceId.split("@")[1] : ""
        liveApplets: true
        notificationCenterAppletAccess: root.notificationCenterAppletAccess
        audioAppletAccess: root.audioAppletAccess
        bluetoothAppletAccess: root.bluetoothAppletAccess
        smartLightsAppletAccess: root.smartLightsAppletAccess
        voiceAppletAccess: root.voiceAppletAccess
        obsAppletAccess: root.obsAppletAccess
        gatherOverviewAccess: root.gatherOverviewAccess
        clipboardAppletAccess: root.clipboardAppletAccess
        powerAppletAccess: root.powerAppletAccess
        launcherAppletAccess: root.launcherAppletAccess
        globalMenuAppletAccess: root.globalMenuAppletAccess
        taskListAppletAccess: root.taskListAppletAccess
        statusNotifierAppletAccess: root.statusNotifierAppletAccess
        desktopControlsAccess: root.desktopControlsAccess
    }
}
