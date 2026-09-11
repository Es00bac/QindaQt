// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Show-desktop toggle over the workspace facade: the button reflects the
// compositor's showingDesktop truth and requests the opposite state.
Item {
    id: root

    required property var access
    property bool vertical: false
    // Worn Luna dressing (ADR-0124), set by the Luna taskbar dispatcher.
    property bool luna: false

    readonly property bool ready: access !== null && Tokens.ready
    readonly property bool showing: ready && Boolean(access.showingDesktop)

    objectName: "showDesktopApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    // AGENT-NOTE: DesktopControlButton paints token-colored glyphs that vanish
    // on the Luna gradient (ADR-0124, "Luna taskbar rendering"). A Luna
    // instance keeps that button as the only input and accessibility surface,
    // fully transparent, and paints a white glyph plus the pressed, shown,
    // and focus states beneath it from the button's own state; the Luna chip
    // supplies the hover tint. Without `luna` nothing here is visible.
    Rectangle {
        objectName: "showDesktopLunaState"
        anchors.fill: parent
        visible: root.luna
        radius: Tokens.radius.m
        color: button.down ? Qt.rgba(0, 0, 0, 0.2)
             : root.showing ? Qt.rgba(1, 1, 1, 0.16)
             : "transparent"

        C.FocusRing {
            anchors.fill: parent
            control: button
        }
    }

    ShellIcons.Icon {
        objectName: "showDesktopLunaIcon"
        anchors.centerIn: parent
        visible: root.luna
        name: "user-desktop"
        size: button.iconExtent
        color: "white"
        symbolic: true
        opacity: button.enabled ? 1.0 : 0.5
        fallbackText: qsTr("Desktop")
        Accessible.ignored: true
    }

    DesktopControlButton {
        id: button
        objectName: "showDesktopButton"
        anchors.fill: parent
        opacity: root.luna ? 0 : 1
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
