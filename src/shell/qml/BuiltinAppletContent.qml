// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaQt.Shell.AudioApplet 1.0 as AudioAppletModule
import QindaQt.Shell.BluetoothApplet 1.0 as BluetoothAppletModule
import QindaQt.Shell.ClipboardApplet 1.0 as ClipboardAppletModule
import QindaQt.Shell.GlobalMenu 1.0 as GlobalMenuModule
import QindaQt.Shell.Launcher 1.0 as LauncherModule
import QindaQt.Shell.PowerApplet 1.0 as PowerAppletModule
import QindaQt.Shell.TaskList 1.0 as TaskListModule
import QindaQt.Shell.StatusNotifier 1.0 as StatusNotifierModule

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
    property var taskListAppletAccess: null
    property var statusNotifierAppletAccess: null
    property var desktopControlsAccess: null
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    property bool dockZoomEnabled: true
    property bool dockHasLauncherGroup: false
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
    readonly property bool taskListPreview:
        !liveApplets && String(applet.plugin ?? "") === "task-list"
    readonly property bool taskListReady:
        (ready && entryPoint === "qindaqt.applets.task-list") || taskListPreview
    readonly property bool statusNotifierPreview:
        !liveApplets && String(applet.plugin ?? "") === "status-notifier"
    readonly property bool statusNotifierReady:
        (ready && entryPoint === "qindaqt.applets.status-notifier") || statusNotifierPreview
    readonly property Component desktopControlComponent:
        ready ? desktopComponents.componentForEntryPoint(entryPoint) : null
    readonly property bool desktopControlReady: desktopControlComponent !== null
    readonly property bool hasLiveContent:
        clockReady || notificationCenterReady || audioReady || bluetoothReady
        || powerReady || clipboardReady || launcherReady || globalMenuReady
        || taskListReady || statusNotifierReady || desktopControlReady
    readonly property bool selected:
        notificationCenterReady && notificationCenterAppletAccess !== null
        && Boolean(notificationCenterAppletAccess.centerOpen)

    // AGENT-CONTRACT: BuiltinAppletRegistry is the compiled trust root; this
    // dispatcher is only its presentation inventory. Focused tests must fail
    // if a registered entry point lacks a renderer here.
    implicitWidth: renderer.item ? renderer.item.implicitWidth : 0
    implicitHeight: renderer.item ? renderer.item.implicitHeight : 0

    DesktopControlsAppletComponents {
        id: desktopComponents
        access: root.desktopControlsAccess
        vertical: root.vertical
        dockMode: root.dockMode
        dockTileSize: root.dockTileSize
        reducedMotion: root.reducedMotion
        dockZoomEnabled: root.dockZoomEnabled
    }

    Loader {
        id: renderer
        anchors.fill: parent
        sourceComponent: root.clockReady ? clockComponent
            : root.notificationCenterReady ? notificationsComponent
            : root.audioReady ? audioComponent
            : root.bluetoothReady ? bluetoothComponent
            : root.powerReady ? powerComponent
            : root.clipboardReady ? clipboardComponent
            : root.launcherReady ? launcherComponent
            : root.globalMenuReady ? globalMenuComponent
            : root.taskListReady ? taskListComponent
            : root.statusNotifierReady ? statusNotifierComponent : root.desktopControlComponent
    }

    Component {
        id: clockComponent
        ClockApplet {
            anchors.fill: parent
            visible: root.clockReady
            applet: root.applet
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: notificationsComponent
        NotificationCenterApplet {
            anchors.fill: parent
            visible: root.notificationCenterReady
            access: root.notificationCenterAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: powerComponent
        PowerAppletModule.PowerApplet {
            anchors.fill: parent
            visible: root.powerReady
            access: root.powerAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: audioComponent
        AudioAppletModule.AudioApplet {
            anchors.fill: parent
            visible: root.audioReady
            controller: root.audioAppletAccess
            vertical: root.vertical
        }
    }

    Component {
        id: bluetoothComponent
        BluetoothAppletModule.BluetoothApplet {
            anchors.fill: parent
            visible: root.bluetoothReady
            access: root.bluetoothAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: clipboardComponent
        ClipboardAppletModule.ClipboardPanelApplet {
            anchors.fill: parent
            visible: root.clipboardReady
            controller: root.clipboardAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: launcherComponent
        LauncherModule.LauncherApplet {
            anchors.fill: parent
            visible: root.launcherReady
            access: root.launcherAppletAccess
            vertical: root.vertical
            dockMode: root.dockMode
            dockTileSize: root.dockTileSize
            reducedMotion: root.reducedMotion
            dockHasLauncherGroup: root.dockHasLauncherGroup
        }
    }

    Component {
        id: globalMenuComponent
        GlobalMenuModule.GlobalMenuApplet {
            anchors.fill: parent
            visible: root.globalMenuReady
            access: root.globalMenuAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: taskListComponent
        TaskListModule.TaskListApplet {
            anchors.fill: parent
            visible: root.taskListReady
            access: root.taskListAppletAccess
            vertical: root.vertical
            dockMode: root.dockMode
            dockTileSize: root.dockTileSize
            dockHasLauncherGroup: root.dockHasLauncherGroup
            reducedMotion: root.reducedMotion
            dockZoomEnabled: root.dockZoomEnabled
        }
    }

    Component {
        id: statusNotifierComponent
        StatusNotifierModule.StatusNotifierApplet {
            anchors.fill: parent
            visible: root.statusNotifierReady
            access: root.statusNotifierAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }
}
