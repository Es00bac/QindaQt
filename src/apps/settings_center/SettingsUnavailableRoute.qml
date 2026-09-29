// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaQt.Tokens 1.0
import QindaQt.Controls 1.0 as Controls

// The explicit fail-closed page shown when no route loader was chosen
// (SettingsRouteHost's single loadability oracle) or a route is unavailable.
Item {
    required property var navigation
    readonly property Item firstFocusTarget: unavailableNotice
    // The navigation object can be absent while the window is torn down; a
    // missing reason then reads as empty text instead of a TypeError.
    readonly property string unavailableReason:
        navigation?.activeRouteUnavailableReason ?? ""

    Controls.DegradedNotice {
        id: unavailableNotice
        objectName: "settingsUnavailableNotice"
        anchors.centerIn: parent
        width: Math.min(parent.width - Tokens.space["6"] * 2, 380)
        reason: unavailableReason.length > 0
            ? unavailableReason
            : qsTr("This settings page is unavailable.")
    }
}
