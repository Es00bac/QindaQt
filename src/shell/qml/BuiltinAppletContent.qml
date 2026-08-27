// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root

    required property var applet
    required property var theme
    property bool vertical: false
    property bool liveApplets: false
    property var notificationCenterAppletAccess: null
    readonly property var runtime: applet.runtime ?? ({})
    readonly property string entryPoint: String(runtime.entryPoint ?? "")
    readonly property bool ready: liveApplets && runtime.ready === true
    readonly property bool clockReady:
        ready && entryPoint === "qindaqt.applets.clock"
    readonly property bool notificationCenterReady:
        ready && entryPoint === "qindaqt.applets.notification-center"
    readonly property bool hasLiveContent: clockReady || notificationCenterReady
    readonly property bool selected:
        notificationCenterReady && notificationCenterAppletAccess !== null
        && Boolean(notificationCenterAppletAccess.centerOpen)

    // AGENT-CONTRACT: BuiltinAppletRegistry is the compiled trust root; this
    // dispatcher is only its presentation inventory. Focused tests must fail
    // if a registered entry point lacks a renderer here.
    implicitWidth: clockReady ? clock.implicitWidth
                   : notificationCenterReady ? notifications.implicitWidth : 0
    implicitHeight: clockReady ? clock.implicitHeight
                    : notificationCenterReady ? notifications.implicitHeight : 0

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
}
