// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The window's non-visual owners and its two secondary windows: the
// Connect-to-server dialog (ADR-0194), the Preferences window (ADR-0198), the
// binding that applies the presentation preferences to this window, and the
// one that starts and stops discovery (ADR-0197).
//
// Extracted from Main.qml so the window stays within the source-shape budget.
// Main drives it through openConnectDialog()/openPreferences() rather than
// reaching for the ids, so there is one entry point per surface.
Item {
    id: root

    required property var navigationController
    required property var networkLocationsController
    required property var preferencesController
    required property var discoveryController
    required property var mountManager

    // Opens the Connect-to-server dialog, empty or pre-filled from a
    // canonical address.
    function openConnectDialog(address) {
        connectToServerDialog.prepare(address)
        connectToServerDialog.open()
    }

    function openPreferences() {
        // No raise(): a Wayland client cannot raise itself, and the offscreen
        // platform warns when asked to, which the QML rows treat as fatal.
        // show() plus requestActivate() is what the compositor acts on.
        preferencesWindow.show()
        preferencesWindow.requestActivate()
    }

    ConnectToServerDialog {
        id: connectToServerDialog
        parent: root.parent
        networkLocationsController: root.networkLocationsController
        preferencesController: root.preferencesController
        onSaved: (url) => root.navigationController.navigateTo(url)
    }

    PresentationDefaults {
        preferencesController: root.preferencesController
        navigationController: root.navigationController
    }

    // Browsing is a network activity the user opts into, so the preference --
    // not the hub being shown -- is what starts and stops it.
    Connections {
        target: root.preferencesController
        function onPreferencesChanged() {
            root.discoveryController.setEnabled(
                root.preferencesController.discoverNearbyServers)
        }
    }

    PreferencesWindow {
        id: preferencesWindow
        preferencesController: root.preferencesController
        discoveryController: root.discoveryController
        networkLocationsController: root.networkLocationsController
        mountManager: root.mountManager
    }
}
