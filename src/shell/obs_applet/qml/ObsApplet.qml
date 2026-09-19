// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
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
    implicitWidth: vertical ? 32 : Math.max(32, summaryContent.implicitWidth + 8)
    implicitHeight: 28

    ToolButton {
        id: summary
        objectName: "obsAppletSummary"
        anchors.fill: parent
        focusPolicy: Qt.TabFocus
        hoverEnabled: true
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
        ToolTip.visible: hovered
        ToolTip.text: Accessible.description

        contentItem: RowLayout {
            id: summaryContent
            spacing: 4

            ShellIcons.Icon {
                objectName: "obsAppletIcon"
                name: root.available ? root.access.iconName : "camera-video-symbolic"
                size: Math.min(20, root.height - 8)
                color: root.live ? Tokens.accent.default : Tokens.fg.default
                symbolic: true
                fallbackText: qsTr("OBS")
                Accessible.ignored: true
            }

            C.Label {
                visible: !root.vertical
                Layout.maximumWidth: 160
                text: root.available ? root.access.summaryLabel : qsTr("OBS")
                font.pointSize: Tokens.type.caption
                color: root.live ? Tokens.accent.default : Tokens.fg.default
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
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
        width: Math.min(360, Math.max(1, Screen.width - 24))
        // Popup.Window sizes its native surface from implicitHeight, including
        // later scene-list changes. Cap that value and scroll the viewport.
        implicitHeight: Math.min(520, popupContent.implicitHeight + topPadding + bottomPadding,
                                 Math.max(1, Screen.height - 48))
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Tokens.radius.m
            color: Tokens.bg.raised
            border.color: Tokens.outline.strong
        }

        contentItem: ScrollView {
            contentWidth: availableWidth
            contentHeight: popupContent.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
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
