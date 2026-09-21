// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The Windows & workspaces route loader (ADR-0210). Like Input, the page takes
// its model from the WindowsRouteComposition singleton inside its own
// component, which is always present when the module is imported and presents
// its own degraded truth when Settings1 is unreachable.
Loader {
    id: loader

    required property var host

    objectName: host.objectNamePrefix + "WindowsLoader"
    anchors.fill: parent
    active: host.presentationActive
            && !host.customizeDeparturePending
            && Boolean(host.navigation?.activeRouteAvailable)
            && host.navigation.activeRouteComponent === "windows"
            && host.windowsComponent !== null
    sourceComponent: host.windowsComponent
}
