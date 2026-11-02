// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Panel QindaLink applet (docs/wiki/shell/link-applet.md). All truth comes
// from the shell-private controller over the public org.qindaqt.Link1
// client; this file only presents it.
//
// AGENT-GUARD: every phone-supplied string (name, model) is rendered with
// Text.PlainText. The pairing code is shown beside them so the user can
// compare it with the phone; Accept is never the default focus.
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property bool available: access !== null
    readonly property string linkState: available ? access.state : "unavailable"
    readonly property string tooltipText: available
        ? access.stateText : qsTr("QindaLink is not running.")
    readonly property color iconColor: {
        switch (root.available ? access.iconColorRole : "muted") {
        case "danger": return Tokens.danger.default
        case "warning": return Tokens.status.warning.foreground
        case "success": return Tokens.status.success.foreground
        default: return Tokens.fg.muted
        }
    }
    readonly property bool showBattery:
        linkState === "connected" && access.batteryText.length > 0
    readonly property int unread: available ? access.unreadCount : 0
    property bool confirmingUnpair: false

    objectName: "linkApplet"
    implicitWidth: showBattery && !vertical ? 64 : 32
    implicitHeight: 28

    C.PanelPopup {
        id: details
        anchorItem: summary
        vertical: root.vertical
        objectName: "linkAppletPopup"
        width: Math.min(320, Math.max(1, outputWidth() - 24))
        padding: 12

        onAboutToShow: {
            root.confirmingUnpair = false
            if (root.available) {
                root.access.refreshAppAvailability()
            }
        }
        onOpened: {
            // Pairing: land on Reject so a stray Enter never approves.
            if (root.linkState === "pairing-request") {
                rejectButton.forceActiveFocus(Qt.PopupFocusReason)
            }
        }
        onClosed: summary.forceActiveFocus(Qt.PopupFocusReason)

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }

        contentItem: ColumnLayout {
            spacing: 8

            Shortcut {
                sequence: "Escape"
                context: Qt.WindowShortcut
                enabled: details.opened
                onActivated: details.close()
            }

            Label {
                objectName: "linkAppletHeading"
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: root.linkState === "pairing-request" ? qsTr("Pair this phone?")
                    : root.available && access.deviceName.length > 0 ? access.deviceName
                    : qsTr("QindaLink")
                font.bold: true
                color: Tokens.fg.default
                wrapMode: Text.Wrap
                Accessible.role: Accessible.Heading
            }

            Label {
                objectName: "linkAppletStateText"
                Layout.fillWidth: true
                visible: root.linkState !== "pairing-request"
                textFormat: Text.PlainText
                text: root.tooltipText
                color: Tokens.fg.muted
                wrapMode: Text.Wrap
            }

            // Pairing request: phone identity, then the code to compare.
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.linkState === "pairing-request"
                spacing: 6

                Label {
                    objectName: "linkAppletPairingPhone"
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: root.available
                        ? (access.pairingModel.length > 0
                           ? qsTr("%1 (%2)").arg(access.pairingName).arg(access.pairingModel)
                           : access.pairingName)
                        : ""
                    color: Tokens.fg.default
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("Make sure this code matches the one on your phone.")
                    color: Tokens.fg.muted
                    wrapMode: Text.Wrap
                }
                Label {
                    objectName: "linkAppletPairingCode"
                    Layout.alignment: Qt.AlignHCenter
                    textFormat: Text.PlainText
                    text: root.available ? access.pairingCode : ""
                    font.bold: true
                    font.pointSize: Tokens.type.title
                    font.letterSpacing: 2
                    color: Tokens.fg.default
                    Accessible.name: qsTr("Pairing code %1").arg(text)
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    C.Button {
                        id: rejectButton
                        objectName: "linkAppletRejectButton"
                        text: qsTr("Reject")
                        emphasized: false
                        enabled: root.available && access.canControl
                        accessibleDescription: qsTr("Refuse to pair with this phone")
                        onClicked: {
                            root.access.rejectPairing()
                            details.close()
                        }
                    }
                    C.Button {
                        id: acceptButton
                        objectName: "linkAppletAcceptButton"
                        text: qsTr("Accept")
                        emphasized: false
                        enabled: root.available && access.canControl
                        accessibleDescription: qsTr("Pair with this phone; the code must match")
                        onClicked: {
                            root.access.acceptPairing()
                            details.close()
                        }
                    }
                }
            }

            // Connected: battery and network at a glance.
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.linkState === "connected"
                spacing: 2
                Label {
                    objectName: "linkAppletBattery"
                    Layout.fillWidth: true
                    visible: root.available && access.batteryText.length > 0
                    textFormat: Text.PlainText
                    text: root.available
                        ? (access.charging ? qsTr("Battery %1, charging").arg(access.batteryText)
                                           : qsTr("Battery %1").arg(access.batteryText))
                        : ""
                    color: Tokens.fg.default
                }
                Label {
                    objectName: "linkAppletNetwork"
                    Layout.fillWidth: true
                    visible: root.available && access.networkText.length > 0
                    textFormat: Text.PlainText
                    text: root.available ? access.networkText : ""
                    color: Tokens.fg.default
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.linkState !== "pairing-request" && !root.confirmingUnpair
                spacing: 6

                C.Button {
                    objectName: "linkAppletRingButton"
                    visible: root.linkState === "connected" || root.linkState === "offline"
                    text: qsTr("Ring phone")
                    emphasized: false
                    enabled: root.available && access.canRing
                    accessibleDescription: enabled
                        ? qsTr("Make the phone ring so you can find it")
                        : qsTr("Unavailable: the phone is not connected or does not support ringing")
                    onClicked: {
                        root.access.ringPhone()
                        details.close()
                    }
                }
                C.Button {
                    objectName: "linkAppletOpenButton"
                    visible: root.available && access.canOpenApp
                    text: qsTr("Open QindaLink")
                    emphasized: root.linkState === "no-phone"
                    accessibleDescription: qsTr("Open the QindaLink window")
                    onClicked: {
                        root.access.openApp()
                        details.close()
                    }
                }
                C.Button {
                    objectName: "linkAppletUnpairButton"
                    visible: root.available && access.hasPhone
                        && root.linkState !== "unavailable"
                    enabled: root.available && access.canControl
                    text: qsTr("Unpair")
                    emphasized: false
                    accessibleDescription: qsTr("Forget this phone")
                    onClicked: root.confirmingUnpair = true
                }
            }

            // Unpair needs a second, explicit step.
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.confirmingUnpair
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("Forget this phone? You will need to pair it again.")
                    color: Tokens.fg.default
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    spacing: 6
                    C.Button {
                        id: cancelUnpairButton
                        objectName: "linkAppletCancelUnpairButton"
                        text: qsTr("Keep phone")
                        emphasized: false
                        onClicked: root.confirmingUnpair = false
                    }
                    C.Button {
                        objectName: "linkAppletConfirmUnpairButton"
                        text: qsTr("Unpair")
                        emphasized: false
                        accessibleDescription: qsTr("Forget this phone now")
                        onClicked: {
                            root.access.unpairPhone()
                            root.confirmingUnpair = false
                            details.close()
                        }
                    }
                }
            }
        }
    }

    ToolButton {
        id: summary
        objectName: "linkAppletSummary"
        anchors.fill: parent
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("QindaLink: %1").arg(root.tooltipText)
        ToolTip.visible: hovered
        ToolTip.text: root.tooltipText

        function openDetails() {
            if (details.opened) {
                details.close()
            } else {
                details.open()
            }
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: Item {
            implicitWidth: chipRow.implicitWidth
            implicitHeight: chipRow.implicitHeight

          Row {
            id: chipRow
            spacing: 4
            anchors.centerIn: parent

            Item {
                id: glyphBox
                implicitWidth: glyph.implicitWidth
                implicitHeight: glyph.implicitHeight
                width: implicitWidth
                height: implicitHeight
                anchors.verticalCenter: parent.verticalCenter

                // A fixed ring (never an animation) while a code waits.
                Rectangle {
                    visible: root.linkState === "pairing-request"
                    anchors.centerIn: glyph
                    width: glyph.width + 10
                    height: glyph.height + 10
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Tokens.status.warning.foreground
                    Accessible.ignored: true
                }

                ShellIcons.Icon {
                    id: glyph
                    objectName: "linkAppletIcon"
                    anchors.centerIn: parent
                    name: "phone-symbolic"
                    size: Math.min(20, root.height - 8)
                    color: root.iconColor
                    symbolic: true
                    fallbackText: qsTr("Phone")
                    Accessible.ignored: true
                }

                // Badge slot for the SMS lane; hidden while the count is 0.
                Rectangle {
                    objectName: "linkAppletBadge"
                    visible: root.unread > 0
                    width: Math.max(14, badgeLabel.implicitWidth + 6)
                    height: 14
                    radius: height / 2
                    color: Tokens.danger.default
                    anchors.top: glyph.top
                    anchors.right: glyph.right
                    anchors.topMargin: -4
                    anchors.rightMargin: -6
                    Accessible.ignored: true

                    Label {
                        id: badgeLabel
                        anchors.centerIn: parent
                        textFormat: Text.PlainText
                        text: root.unread > 99 ? qsTr("99+") : String(root.unread)
                        color: Tokens.danger.fg
                        font.pointSize: Tokens.type.caption
                        font.bold: true
                    }
                }
            }

            Label {
                id: batteryLabel
                objectName: "linkAppletBatteryLabel"
                visible: root.showBattery && !root.vertical
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: root.showBattery
                    ? access.batteryText + (access.charging ? "⚡" : "") : ""
                color: Tokens.fg.default
                font.pointSize: Tokens.type.caption
                Accessible.ignored: true
            }
          }
        }
        background: Item {}
    }
}
