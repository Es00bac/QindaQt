// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// The OBS applet: a top-bar chip saying what OBS is doing, and a popup with
// the controls.
//
// AGENT-GUARD: the chip takes the accent colour exactly while OBS is
// recording or streaming. A user who is live needs to see that from across
// the room, and a chip that looked the same either way would be worse than
// no chip at all.
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null
    readonly property bool live: root.available
                                 && (root.access.recording || root.access.streaming)

    objectName: "obsApplet"
    implicitWidth: 32
    implicitHeight: 28

    ToolButton {
        id: summary
        objectName: "obsAppletSummary"
        anchors.fill: parent
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.available ? root.access.accessibleName : qsTr("OBS")
        Accessible.description: root.available
                                ? root.access.accessibleDescription
                                : qsTr("OBS support is not available in this session.")

        function openDetails() {
            details.open()
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: RowLayout {
            spacing: 3

            ShellIcons.Icon {
                objectName: "obsAppletIcon"
                name: root.available ? root.access.iconName : "camera-video-symbolic"
                size: Math.min(20, root.height - 8)
                color: root.live ? Tokens.accent.default : Tokens.fg.default
                symbolic: true
                fallbackText: qsTr("OBS")
                Accessible.ignored: true
            }
        }
        background: Item {}
    }

    Popup {
        id: details
        // AGENT-GUARD: panels reject keyboard focus and cannot paint outside
        // their surface. A separate popup window supplies both capabilities.
        popupType: Popup.Window
        objectName: "obsAppletPopup"
        width: 360
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"
        }

        contentItem: ScrollView {
            implicitHeight: Math.min(popupContent.implicitHeight, 520)
            clip: true

            // AGENT-GUARD: shortcut ownership follows the focusable popup
            // content window, never the non-focusable panel.
            Shortcut {
                sequence: "Escape"
                context: Qt.WindowShortcut
                enabled: details.opened
                onActivated: details.close()
            }

            ObsAppletPopup {
                id: popupContent
                width: details.availableWidth
                controller: root.access
            }
        }
    }
}
