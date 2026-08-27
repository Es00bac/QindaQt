// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

AbstractButton {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available:
        access !== null && Boolean(access.privatePresentationAllowed)

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
            // A text glyph keeps the first implementation themeable and avoids
            // granting the panel applet any icon-path or image-loading authority.
            text: !root.available ? "○"
                  : root.access?.doNotDisturbEnabled ? "☾"
                  : root.access?.centerOpen ? "●" : "◉"
            color: !root.available ? (root.colors.textMuted ?? "#a9afa9")
                   : root.down ? (root.colors.accentText ?? "#10201b")
                   : (root.colors.text ?? "white")
            font.pixelSize: root.vertical ? 17 : 15
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
        }
    }

    background: Item {}
}
