// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls

// The explicit fail-closed page shown when no route loader was chosen
// (SettingsRouteHost's single loadability oracle) or a route is unavailable.
Item {
    required property var navigation
    readonly property Item firstFocusTarget: unavailableNotice

    Controls.DegradedNotice {
        id: unavailableNotice
        objectName: "settingsUnavailableNotice"
        anchors.centerIn: parent
        width: Math.min(parent.width - Tokens.space["6"] * 2, 380)
        reason: navigation.activeRouteUnavailableReason.length > 0
            ? navigation.activeRouteUnavailableReason
            : qsTr("This settings page is unavailable.")
    }
}
