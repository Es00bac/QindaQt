// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaQt.Shell.DesktopControls 1.0 as DesktopControlsModule

// AGENT-CONTRACT: This inventory creates Component definitions, never applet
// instances. BuiltinAppletContent's sole Loader instantiates one selected
// definition. Each definition receives its purpose-specific public facade.
QtObject {
    id: root
    required property var access
    property bool vertical: false
    property bool dockMode: false
    property int dockTileSize: 60
    property bool reducedMotion: false
    property bool dockZoomEnabled: true

    function facade(name) {
        if (access === null || access === undefined)
            return null
        if (name === "dashboard")
            return { systemStatus: access.systemStatus ?? null,
                     workspaces: access.workspaces ?? null, launcher: access.launcher ?? null }
        return access[name] ?? null
    }

    function componentForEntryPoint(entryPoint) {
        switch (entryPoint) {
        case "qindaqt.applets.workspace-switcher": return workspaceSwitcherComponent
        case "qindaqt.applets.workspace-tiles": return workspaceTilesComponent
        case "qindaqt.applets.show-desktop": return showDesktopComponent
        case "qindaqt.applets.overview-trigger": return overviewComponent
        case "qindaqt.applets.active-application": return activeApplicationComponent
        case "qindaqt.applets.system-menu": return systemMenuComponent
        case "qindaqt.applets.system-status": return systemStatusComponent
        case "qindaqt.applets.places-menu": return placesComponent
        case "qindaqt.applets.quick-launch": return quickLaunchComponent
        case "qindaqt.applets.application-tiles": return applicationTilesComponent
        case "qindaqt.applets.command-palette": return commandPaletteComponent
        case "qindaqt.applets.command-hud": return commandHudComponent
        case "qindaqt.applets.dashboard": return dashboardComponent
        default: return null
        }
    }

    readonly property Component workspaceSwitcherComponent: Component {
        DesktopControlsModule.WorkspaceSwitcherApplet {
            anchors.fill: parent
            access: root.facade("workspaces")
            vertical: root.vertical
        }
    }

    readonly property Component workspaceTilesComponent: Component {
        DesktopControlsModule.WorkspaceTilesApplet {
            anchors.fill: parent
            access: root.facade("workspaces")
            vertical: root.vertical
        }
    }

    readonly property Component showDesktopComponent: Component {
        DesktopControlsModule.ShowDesktopApplet {
            anchors.fill: parent
            access: root.facade("workspaces")
            vertical: root.vertical
        }
    }

    readonly property Component overviewComponent: Component {
        DesktopControlsModule.OverviewTriggerApplet {
            anchors.fill: parent
            access: root.facade("overview")
            vertical: root.vertical
        }
    }

    readonly property Component activeApplicationComponent: Component {
        DesktopControlsModule.ActiveApplicationApplet {
            anchors.fill: parent
            access: root.facade("activeApplication")
            vertical: root.vertical
        }
    }

    readonly property Component systemMenuComponent: Component {
        DesktopControlsModule.SystemMenuApplet {
            anchors.fill: parent
            access: root.facade("systemMenu")
            vertical: root.vertical
        }
    }

    readonly property Component systemStatusComponent: Component {
        DesktopControlsModule.SystemStatusApplet {
            anchors.fill: parent
            access: root.facade("systemStatus")
            vertical: root.vertical
        }
    }

    readonly property Component placesComponent: Component {
        DesktopControlsModule.PlacesMenuApplet {
            anchors.fill: parent
            access: root.facade("places")
            vertical: root.vertical
        }
    }

    readonly property Component quickLaunchComponent: Component {
        DesktopControlsModule.QuickLaunchApplet {
            anchors.fill: parent
            access: root.facade("quickLaunch")
            vertical: root.vertical
            dockMode: root.dockMode
            dockTileSize: root.dockTileSize
            reducedMotion: root.reducedMotion
            dockZoomEnabled: root.dockZoomEnabled
        }
    }

    readonly property Component applicationTilesComponent: Component {
        DesktopControlsModule.ApplicationTilesApplet {
            anchors.fill: parent
            access: root.facade("quickLaunch")
            vertical: root.vertical
        }
    }

    readonly property Component commandPaletteComponent: Component {
        DesktopControlsModule.CommandPaletteApplet {
            anchors.fill: parent
            access: root.facade("commandPalette")
            vertical: root.vertical
        }
    }

    readonly property Component commandHudComponent: Component {
        DesktopControlsModule.CommandHudApplet {
            anchors.fill: parent
            access: root.facade("commandHud")
            vertical: root.vertical
        }
    }

    readonly property Component dashboardComponent: Component {
        DesktopControlsModule.DashboardApplet {
            anchors.fill: parent
            access: root.facade("dashboard")
            vertical: root.vertical
        }
    }
}
