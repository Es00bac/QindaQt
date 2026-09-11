// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QindaQt.Shell.Icons 1.0 as ShellIcons

AbstractButton {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    // Worn Luna dressing (ADR-0124), set by the Luna taskbar dispatcher.
    property bool luna: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available:
        access !== null && Boolean(access.privatePresentationAllowed)
    readonly property bool doNotDisturbShown:
        available && Boolean(access.doNotDisturbEnabled)
    // AGENT-NOTE: on the Luna taskbar the text glyph reads as a placeholder
    // dot, so the Luna path shows the icon theme's `notifications` glyph in
    // white (ADR-0124, "Luna taskbar rendering"). Do Not Disturb keeps its
    // crescent text glyph, in white, because the icon theme ships no muted
    // bell. The standard path keeps the themeable text glyph.
    readonly property bool showsLunaIcon: luna && !doNotDisturbShown

    objectName: "notificationCenterApplet"
    implicitWidth: vertical ? 40 : 36
    implicitHeight: vertical ? 40 : 28
    enabled: available
    checkable: false
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: !available
                     ? qsTr("Notifications unavailable")
                     : access.doNotDisturbEnabled
                       ? access.centerOpen
                         ? qsTr("Close notification center; Do Not Disturb is on")
                         : qsTr("Open notification center; Do Not Disturb is on")
                       : access.centerOpen
                         ? qsTr("Close notification center")
                         : qsTr("Open notification center")

    function activate() {
        if (available)
            access.toggle();
    }

    onClicked: activate()
    Accessible.onPressAction: activate()

    contentItem: Item {
        Text {
            objectName: "notificationCenterAppletGlyph"
            anchors.centerIn: parent
            visible: !root.showsLunaIcon
            // A text glyph keeps the standard path themeable without any
            // icon lookup; the Luna icon below uses the confined shell icon
            // provider, which resolves names only.
            text: !root.available ? "○"
                  : root.access?.doNotDisturbEnabled ? "☾"
                  : root.access?.centerOpen ? "●" : "◉"
            color: root.luna ? "white"
                   : !root.available ? (root.colors.textMuted ?? "#a9afa9")
                   : root.down ? (root.colors.accentText ?? "#10201b")
                   : (root.colors.text ?? "white")
            font.pixelSize: root.vertical ? 17 : 15
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
        }

        ShellIcons.Icon {
            objectName: "notificationCenterAppletIcon"
            anchors.centerIn: parent
            visible: root.showsLunaIcon
            name: "notifications"
            size: root.vertical ? 20 : 16
            color: "white"
            symbolic: true
            opacity: root.available ? 1.0 : 0.5
            fallbackText: qsTr("Notifications")
            Accessible.ignored: true
        }
    }

    background: Item {}
}
