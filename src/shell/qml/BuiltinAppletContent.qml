// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Shell.AudioApplet 1.0 as AudioAppletModule
import QindaQt.Shell.BluetoothApplet 1.0 as BluetoothAppletModule
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
    property var powerAppletAccess: null
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
    readonly property bool hasLiveContent:
        clockReady || notificationCenterReady || audioReady || bluetoothReady
        || powerReady
    readonly property bool selected:
        notificationCenterReady && notificationCenterAppletAccess !== null
        && Boolean(notificationCenterAppletAccess.centerOpen)

    // AGENT-CONTRACT: BuiltinAppletRegistry is the compiled trust root; this
    // dispatcher is only its presentation inventory. Focused tests must fail
    // if a registered entry point lacks a renderer here.
    implicitWidth: clockReady ? clock.implicitWidth
                   : notificationCenterReady ? notifications.implicitWidth
                   : audioReady ? audio.implicitWidth
                   : bluetoothReady ? bluetooth.implicitWidth
                   : powerReady ? power.implicitWidth : 0
    implicitHeight: clockReady ? clock.implicitHeight
                    : notificationCenterReady ? notifications.implicitHeight
                    : audioReady ? audio.implicitHeight
                    : bluetoothReady ? bluetooth.implicitHeight
                    : powerReady ? power.implicitHeight : 0

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
}
