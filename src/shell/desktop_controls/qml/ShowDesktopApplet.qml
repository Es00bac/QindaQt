// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// Show-desktop toggle over the workspace facade: the button reflects the
// compositor's showingDesktop truth and requests the opposite state.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property bool showing: ready && Boolean(access.showingDesktop)

    objectName: "showDesktopApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    DesktopControlButton {
        id: button
        objectName: "showDesktopButton"
        anchors.fill: parent
        iconName: "user-desktop"
        fallbackText: qsTr("Desktop")
        vertical: root.vertical
        available: root.ready && Boolean(root.access.canShowDesktop)
        active: root.showing
        Accessible.name: root.showing ? qsTr("Hide desktop") : qsTr("Show desktop")
        Accessible.checkable: true
        Accessible.checked: root.showing
        accessibleDescription: !root.ready
                               ? qsTr("Desktop controls are not connected")
                               : !Boolean(root.access.canShowDesktop)
                                 ? String(root.access.accessibleDescription)
                                 : qsTr("Toggles whether all windows are hidden")
        onTriggered: if (root.ready) root.access.toggleShowingDesktop()
    }
}
