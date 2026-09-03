// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Shell.AudioApplet 1.0 as AudioAppletModule
import QindaQt.Shell.BluetoothApplet 1.0 as BluetoothAppletModule
import QindaQt.Shell.ClipboardApplet 1.0 as ClipboardAppletModule
import QindaQt.Shell.GlobalMenu 1.0 as GlobalMenuModule
import QindaQt.Shell.Launcher 1.0 as LauncherModule
import QindaQt.Shell.PowerApplet 1.0 as PowerAppletModule

Item {
    id: root

    required property var applet
    required property var theme
    property bool vertical: false
    property bool liveApplets: false
    property var notificationCenterAppletAccess: null
    property var audioAppletAccess: null
    property var bluetoothAppletAccess: null
    property var clipboardAppletAccess: null
    property var powerAppletAccess: null
    property var launcherAppletAccess: null
    property var globalMenuAppletAccess: null
    readonly property var runtime: applet.runtime ?? ({})
    readonly property string entryPoint: String(runtime.entryPoint ?? "")
    readonly property bool ready: liveApplets && runtime.ready === true
    readonly property bool clockReady:
        ready && entryPoint === "qindaqt.applets.clock"
    readonly property bool notificationCenterReady:
        ready && entryPoint === "qindaqt.applets.notification-center"
    readonly property bool audioReady:
        ready && entryPoint === "qindaqt.applets.audio"
    readonly property bool bluetoothReady:
        ready && entryPoint === "qindaqt.applets.bluetooth"
    readonly property bool powerReady:
        ready && entryPoint === "qindaqt.applets.power"
    readonly property bool clipboardPreview:
        !liveApplets && String(applet.plugin ?? "") === "clipboard"
    readonly property bool clipboardReady:
        (ready && entryPoint === "qindaqt.applets.clipboard") || clipboardPreview
    readonly property bool launcherPreview:
        !liveApplets && String(applet.plugin ?? "") === "launcher"
    readonly property bool launcherReady:
        (ready && entryPoint === "qindaqt.applets.launcher") || launcherPreview
    readonly property bool globalMenuReady:
        ready && entryPoint === "qindaqt.applets.global-menu"
    readonly property bool hasLiveContent:
        clockReady || notificationCenterReady || audioReady || bluetoothReady
        || powerReady || clipboardReady || launcherReady || globalMenuReady
    readonly property bool selected:
        notificationCenterReady && notificationCenterAppletAccess !== null
        && Boolean(notificationCenterAppletAccess.centerOpen)

    function inheritedLauncherAccess() {
        let candidate = root.parent
        // AGENT-CONTRACT: AppletChip is outside the launcher lane's ownership.
        // The permitted panel rows carry this one purpose-specific facade;
        // bounded ancestor lookup bridges that existing component without
        // exposing a general shell object or transport to applet QML.
        for (let depth = 0; candidate !== null && depth < 4; ++depth) {
            if (typeof candidate.launcherAppletAccess !== "undefined")
                return candidate.launcherAppletAccess
            candidate = candidate.parent
        }
        return null
    }

    readonly property var effectiveLauncherAppletAccess:
        launcherAppletAccess !== null ? launcherAppletAccess
                                      : inheritedLauncherAccess()

    function inheritedGlobalMenuAccess() {
        let candidate = root.parent
        // AppletChip deliberately remains a presentation-only boundary. The
        // permitted panel rows carry this purpose-specific facade, and this
        // bounded lookup crosses only that existing wrapper.
        for (let depth = 0; candidate !== null && depth < 4; ++depth) {
            if (typeof candidate.globalMenuAppletAccess !== "undefined")
                return candidate.globalMenuAppletAccess
            candidate = candidate.parent
        }
        return null
    }

    readonly property var effectiveGlobalMenuAppletAccess:
        globalMenuAppletAccess !== null ? globalMenuAppletAccess
                                       : inheritedGlobalMenuAccess()

    function inheritedClipboardAccess() {
        let candidate = root.parent
        // AppletChip intentionally remains presentation-only. The panel rows
        // carry this one controller facade, and the bounded lookup crosses
        // only the existing wrapper instead of exposing a shell service bag.
        for (let depth = 0; candidate !== null && depth < 4; ++depth) {
            if (typeof candidate.clipboardAppletAccess !== "undefined")
                return candidate.clipboardAppletAccess
            candidate = candidate.parent
        }
        return null
    }

    readonly property var effectiveClipboardAppletAccess:
        clipboardAppletAccess !== null ? clipboardAppletAccess
                                      : inheritedClipboardAccess()

    // AGENT-CONTRACT: BuiltinAppletRegistry is the compiled trust root; this
    // dispatcher is only its presentation inventory. Focused tests must fail
    // if a registered entry point lacks a renderer here.
    implicitWidth: clockReady ? clock.implicitWidth
                   : notificationCenterReady ? notifications.implicitWidth
                   : audioReady ? audio.implicitWidth
                   : bluetoothReady ? bluetooth.implicitWidth
                   : powerReady ? power.implicitWidth
                   : clipboardReady ? clipboard.implicitWidth
                   : launcherReady ? launcher.implicitWidth
                   : globalMenuReady ? globalMenu.implicitWidth : 0
    implicitHeight: clockReady ? clock.implicitHeight
                    : notificationCenterReady ? notifications.implicitHeight
                    : audioReady ? audio.implicitHeight
                    : bluetoothReady ? bluetooth.implicitHeight
                    : powerReady ? power.implicitHeight
                    : clipboardReady ? clipboard.implicitHeight
                    : launcherReady ? launcher.implicitHeight
                    : globalMenuReady ? globalMenu.implicitHeight : 0

    ClockApplet {
        id: clock
        anchors.fill: parent
        visible: root.clockReady
        applet: root.applet
        theme: root.theme
        vertical: root.vertical
    }

    NotificationCenterApplet {
        id: notifications
        anchors.fill: parent
        visible: root.notificationCenterReady
        access: root.notificationCenterAppletAccess
        theme: root.theme
        vertical: root.vertical
    }

    PowerAppletModule.PowerApplet {
        id: power
        anchors.fill: parent
        visible: root.powerReady
        access: root.powerAppletAccess
        theme: root.theme
        vertical: root.vertical
    }

    AudioAppletModule.AudioApplet {
        id: audio
        anchors.fill: parent
        visible: root.audioReady
        controller: root.audioAppletAccess
    }

    BluetoothAppletModule.BluetoothApplet {
        id: bluetooth
        anchors.fill: parent
        visible: root.bluetoothReady
        access: root.bluetoothAppletAccess
        theme: root.theme
        vertical: root.vertical
    }

    ClipboardAppletModule.ClipboardPanelApplet {
        id: clipboard
        anchors.fill: parent
        visible: root.clipboardReady
        controller: root.effectiveClipboardAppletAccess
        theme: root.theme
        vertical: root.vertical
    }

    LauncherModule.LauncherApplet {
        id: launcher
        anchors.fill: parent
        visible: root.launcherReady
        access: root.effectiveLauncherAppletAccess
        vertical: root.vertical
    }

    GlobalMenuModule.GlobalMenuApplet {
        id: globalMenu
        anchors.fill: parent
        visible: root.globalMenuReady
        access: root.effectiveGlobalMenuAppletAccess
        theme: root.theme
        vertical: root.vertical
    }
}
