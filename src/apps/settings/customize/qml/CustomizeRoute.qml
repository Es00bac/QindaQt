// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick

// The Settings Customize route: layout presets (ADR-0267). Nothing here is
// ever a draft, so leaving the route or closing the window never asks.
Item {
    id: root

    required property var customizeSettings
    // Assigned by SettingsRouteHost; the preset page does not navigate.
    property var navigation: null
    // AGENT-CONTRACT: SettingsRouteHost binds both signals on every Customize
    // instance and calls requestClose() only while the model reports dirty,
    // which the preset model never does (customize_settings_model.h).
    signal closeRequested()
    signal closeCancelled()
    readonly property Item firstFocusTarget: page.firstFocusTarget

    function requestClose() {
        root.closeRequested()
    }

    CustomizePage {
        id: page
        anchors.fill: parent
        customizeSettings: root.customizeSettings
    }
}
