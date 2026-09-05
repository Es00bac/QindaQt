// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Tokens 1.0

// GNOME-style "Activities" trigger. The overview is a separate popup window
// searching windows, workspaces, and applications through their facades;
// it never owns window or launch authority itself.
Item {
    id: root

    required property var access
    property bool vertical: false

    readonly property bool ready: access !== null && Tokens.ready

    objectName: "overviewTriggerApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    DesktopControlButton {
        id: button
        objectName: "overviewTriggerButton"
        anchors.fill: parent
        iconName: "view-grid"
        fallbackText: qsTr("Activities")
        labelText: qsTr("Activities")
        showLabel: true
        vertical: root.vertical
        available: root.ready && Boolean(root.access.available)
        active: overview.opened
        Accessible.name: qsTr("Activities")
        accessibleDescription: available
                               ? qsTr("Opens the window, workspace, and application overview")
                               : qsTr("The overview is unavailable")
        onTriggered: if (available) overview.open()
    }

    CommandSearchPopup {
        id: overview
        objectName: "overviewPopup"
        access: root.access
        heading: qsTr("Overview")
        placeholderText: qsTr("Search windows and applications")
        emptyText: qsTr("No windows, workspaces, or applications match")
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)
    }
}
