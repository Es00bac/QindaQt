// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaQt.Shell.AudioApplet 1.0 as AudioAppletModule
import QindaQt.Shell.BluetoothApplet 1.0 as BluetoothAppletModule
import QindaQt.Shell.ClipboardApplet 1.0 as ClipboardAppletModule
import QindaQt.Shell.GlobalMenu 1.0 as GlobalMenuModule
import QindaQt.Shell.Launcher 1.0 as LauncherModule
import QindaQt.Shell.NetworkApplet 1.0 as NetworkAppletModule
import QindaQt.Shell.PowerApplet 1.0 as PowerAppletModule
import QindaQt.Shell.SmartLightsApplet 1.0 as SmartLightsAppletModule
import QindaQt.Shell.VoiceApplet 1.0 as VoiceAppletModule
import QindaQt.Shell.ObsApplet 1.0 as ObsAppletModule
import QindaQt.Shell.GatherOverview 1.0 as GatherOverviewModule
import QindaQt.Shell.TaskList 1.0 as TaskListModule
import QindaQt.Shell.StatusNotifier 1.0 as StatusNotifierModule
import QindaQt.Shell.StartMenu 1.0 as StartMenuModule

Item {
    id: root

    required property var applet
    required property var theme
    property bool vertical: false
    property bool liveApplets: false
    property var notificationCenterAppletAccess: null
    property var audioAppletAccess: null
    property var bluetoothAppletAccess: null
    property var networkAppletAccess: null
    property var clipboardAppletAccess: null
    property var powerAppletAccess: null
    property var smartLightsAppletAccess: null
    property var voiceAppletAccess: null
    property var obsAppletAccess: null
    property var gatherOverviewAccess: null
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
    // Worn Luna taskbar (ADR-0124): lunaMode is PanelContent's panel-derived
    // dressing and `presentation: "luna"` the per-instance profile opt-in.
    // Either one selects the Luna path of a renderer that has one.
    property bool lunaMode: false
    readonly property bool luna: lunaMode
        || (applet.settings ?? ({})).presentation === "luna"
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
    readonly property bool networkReady:
        ready && entryPoint === "qindaqt.applets.network"
    readonly property bool powerReady:
        ready && entryPoint === "qindaqt.applets.power"
    readonly property bool smartLightsReady:
        ready && entryPoint === "qindaqt.applets.smart-lights"
    readonly property bool voiceReady:
        ready && entryPoint === "qindaqt.applets.voice"
    readonly property bool obsReady:
        ready && entryPoint === "qindaqt.applets.obs"
    readonly property bool gatherOverviewReady:
        ready && entryPoint === "qindaqt.applets.gather-overview"
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
    readonly property bool startMenuReady:
        ready && entryPoint === "qindaqt.applets.start-menu"
    readonly property Component desktopControlComponent:
        ready ? desktopComponents.componentForEntryPoint(entryPoint) : null
    readonly property bool desktopControlReady: desktopControlComponent !== null
    readonly property bool hasLiveContent:
        clockReady || notificationCenterReady || audioReady || bluetoothReady
        || networkReady || powerReady || smartLightsReady || voiceReady || clipboardReady
        || launcherReady || globalMenuReady
        || taskListReady || statusNotifierReady || desktopControlReady
        || startMenuReady
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
        appletSettings: root.applet.settings ?? ({})
    }

    Loader {
        id: renderer
        anchors.fill: parent
        sourceComponent: root.clockReady ? clockComponent
            : root.notificationCenterReady ? notificationsComponent
            : root.audioReady ? audioComponent
            : root.bluetoothReady ? bluetoothComponent
            : root.networkReady ? networkComponent
            : root.powerReady ? powerComponent
            : root.smartLightsReady ? smartLightsComponent
            : root.voiceReady ? voiceComponent
            : root.obsReady ? obsComponent
            : root.gatherOverviewReady ? gatherOverviewComponent
            : root.clipboardReady ? clipboardComponent
            : root.launcherReady ? launcherComponent
            : root.globalMenuReady ? globalMenuComponent
            : root.taskListReady ? taskListComponent
            : root.statusNotifierReady ? statusNotifierComponent
            : root.startMenuReady ? startMenuComponent
            : root.desktopControlComponent

        // AGENT-NOTE: desktop-control renderers come from the separate
        // DesktopControlsAppletComponents inventory, which binds only the
        // settings-driven `presentation` opt-in. A loaded control that
        // declares `luna` (show desktop, quick launch) is rebound to this
        // dispatcher's `luna`, which adds the panel-derived lunaMode
        // (ADR-0124) and equals the inventory's value everywhere else.
        // AGENT-GUARD: test only the property's presence here. Reading its
        // value in a declarative Binding target re-triggered that binding
        // when it wrote `luna` (a binding loop).
        onLoaded: {
            if (root.desktopControlReady && item !== null && ("luna" in item))
                item.luna = Qt.binding(() => root.luna)
        }
    }

    Component {
        id: clockComponent
        ClockApplet {
            anchors.fill: parent
            visible: root.clockReady
            applet: root.applet
            theme: root.theme
            vertical: root.vertical
            luna: root.luna
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
            luna: root.luna
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
        id: smartLightsComponent
        SmartLightsAppletModule.SmartLightsApplet {
            anchors.fill: parent
            visible: root.smartLightsReady
            access: root.smartLightsAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: gatherOverviewComponent
        GatherOverviewModule.GatherOverviewApplet {
            anchors.fill: parent
            visible: root.gatherOverviewReady
            access: root.gatherOverviewAccess
            vertical: root.vertical
        }
    }

    Component {
        id: voiceComponent
        VoiceAppletModule.VoiceApplet {
            anchors.fill: parent
            visible: root.voiceReady
            access: root.voiceAppletAccess
            theme: root.theme
            vertical: root.vertical
        }
    }

    Component {
        id: obsComponent
        ObsAppletModule.ObsApplet {
            anchors.fill: parent
            visible: root.obsReady
            access: root.obsAppletAccess
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
        id: networkComponent
        NetworkAppletModule.NetworkApplet {
            anchors.fill: parent
            visible: root.networkReady
            access: root.networkAppletAccess
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
            luna: root.luna
            grouping: String((root.applet.settings ?? ({})).grouping
                             ?? "when-crowded")
            // ADR-0224: centered-task-list and grouped-task-list are the same
            // implementation under their own manifest names; the tile shape
            // arrives as this setting, defaulted by each manifest.
            presentation: String((root.applet.settings ?? ({})).presentation
                                 ?? "standard")
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
            luna: root.luna
        }
    }

    Component {
        id: startMenuComponent
        StartMenuModule.StartMenuApplet {
            anchors.fill: parent
            visible: root.startMenuReady
            applet: root.applet
            theme: root.theme
            vertical: root.vertical
            launcherAppletAccess: root.launcherAppletAccess
            desktopControlsAccess: root.desktopControlsAccess
        }
    }
}
