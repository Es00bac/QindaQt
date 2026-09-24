// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaTK.QindaQt
import "SettingsSearchCommands.js" as SettingsSearch

// The Ctrl+K Settings search (ADR-0257): QindaTK's command palette over the
// navigation controller's routes, their keywords and deep-link destinations.
//
// AGENT-CONTRACT: this file only reads `navigation.routes` and calls
// `navigation.selectRoute` / `navigation.selectRouteDestination`; it never
// reaches into a route page. Search terms and destinations are registry data
// (settings_route_search_metadata.cpp), so a page learns nothing new here.
Tk.CommandPalette {
    id: palette

    required property var navigation
    // Emitted once the palette has closed after it changed the route or the
    // requested destination, so the host can move focus into the new page
    // after the popup has finished restoring focus.
    signal navigated()

    property bool navigatedBeforeClose: false

    // AGENT-NOTE: QindaTK reads its own Theme; the bridge maps the session's
    // QST-1 tokens onto it, as the Audio route does (ADR-0227). It is a
    // property rather than a child because a Popup's children are content.
    readonly property QtObject themeBridge: QindaQtTheme {}

    readonly property var allCommands: SettingsSearch.buildCommands(
        palette.navigation ? palette.navigation.routes : [], {
            unavailable: qsTr("%1 (unavailable: %2)"),
            destination: qsTr("%1 › %2")
        })

    objectName: "settingsCommandPalette"
    placeholderText: qsTr("Search settings…")
    commands: SettingsSearch.paletteCommands(
        palette.allCommands, palette.filterText, qsTr("Results"))

    function activateCommand(commandId) {
        const command = SettingsSearch.find(palette.allCommands, commandId)
        if (command === null || palette.navigation === null)
            return false
        // AGENT-GUARD: same guard as the sidebar and compact tabs: an
        // unavailable route is listed with its reason but never selected
        // from navigation. It stays reachable only by startup intent.
        if (!command.available)
            return false
        const accepted = command.destination.length > 0
            ? palette.navigation.selectRouteDestination(command.routeId, command.destination)
            : palette.navigation.selectRoute(command.routeId)
        palette.navigatedBeforeClose = accepted
        return accepted
    }

    onActivated: commandId => palette.activateCommand(commandId)
    onAboutToShow: palette.navigatedBeforeClose = false
    onClosed: {
        if (palette.navigatedBeforeClose) {
            palette.navigatedBeforeClose = false
            palette.navigated()
        }
    }
}
