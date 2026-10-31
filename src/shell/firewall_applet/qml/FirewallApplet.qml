// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Panel Firewall applet (docs/wiki/shell/firewall-applet.md, ADR-0431). All
// truth comes from the shell-private controller over the public
// org.qindaqt.Firewall1 client; this file only presents it. The applet
// never changes the firewall: no control here can block, allow, or edit a
// rule, only open the real window, hide an alert, or mark a login as the
// owner's own.
Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property bool available: access !== null
    readonly property color iconColor: {
        switch (root.available ? access.iconColorRole : "muted") {
        case "danger": return Tokens.danger.default
        case "warning": return Tokens.status.warning.foreground
        case "success": return Tokens.status.success.foreground
        default: return Tokens.fg.muted
        }
    }
    readonly property int alertCount: available ? access.alertCount : 0
    readonly property bool badge: alertCount > 0
    readonly property bool persistent: available && access.persistentAttention

    objectName: "firewallApplet"
    implicitWidth: 32
    implicitHeight: 28

    C.PanelPopup {
        id: details
        anchorItem: summary
        vertical: root.vertical
        objectName: "firewallAppletPopup"
        width: Math.min(340, Math.max(1, outputWidth() - 24))
        padding: 12

        onClosed: summary.forceActiveFocus(Qt.PopupFocusReason)

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: Tokens.bg.raised
            border.color: Tokens.outline.divider
        }

        contentItem: ColumnLayout {
            id: contentColumn
            spacing: 8

            Shortcut {
                sequence: "Escape"
                context: Qt.WindowShortcut
                enabled: details.opened
                onActivated: details.close()
            }

            Label {
                objectName: "firewallAppletHeading"
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: root.available && access.hasAlert ? access.alertTitle : qsTr("Firewall")
                font.bold: true
                color: Tokens.fg.default
                wrapMode: Text.Wrap
                Accessible.role: Accessible.Heading
            }

            // No open alert: just the current state, in words.
            Label {
                objectName: "firewallAppletStateText"
                Layout.fillWidth: true
                visible: !root.available || !access.hasAlert
                textFormat: Text.PlainText
                text: root.available ? access.stateText
                                     : qsTr("Background watching is off. Open QindaFirewall to turn it on.")
                color: Tokens.fg.muted
                wrapMode: Text.Wrap
            }

            // An open alert: what happened, is it dangerous, what to do.
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.available && access.hasAlert
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("What happened")
                    font.bold: true
                    color: Tokens.fg.muted
                    Accessible.role: Accessible.Heading
                }
                Label {
                    objectName: "firewallAppletSummary"
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: root.available ? access.alertSummary : ""
                    color: Tokens.fg.default
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("Is this dangerous?")
                    font.bold: true
                    color: Tokens.fg.muted
                    Accessible.role: Accessible.Heading
                }
                Label {
                    objectName: "firewallAppletMeaning"
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: root.available ? access.alertMeaning : ""
                    color: Tokens.fg.default
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("What should I do?")
                    font.bold: true
                    color: Tokens.fg.muted
                    Accessible.role: Accessible.Heading
                }
                Label {
                    objectName: "firewallAppletAdvice"
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: root.available ? access.alertAdvice : ""
                    color: Tokens.fg.default
                    wrapMode: Text.Wrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                C.Button {
                    objectName: "firewallAppletOpenButton"
                    text: qsTr("Open QindaFirewall")
                    emphasized: !root.available || !access.hasAlert
                    accessibleDescription: qsTr("Open the QindaFirewall window")
                    onClicked: {
                        if (root.available) {
                            access.openWindow()
                        }
                        details.close()
                    }
                }
                C.Button {
                    objectName: "firewallAppletDismissButton"
                    visible: root.available && access.hasAlert
                    text: qsTr("Got it, hide this")
                    emphasized: false
                    accessibleDescription: qsTr("Hide this alert")
                    onClicked: {
                        access.dismissTopAlert()
                        details.close()
                    }
                }
                C.Button {
                    objectName: "firewallAppletConfirmMineButton"
                    visible: root.available && access.hasAlert && access.alertCanConfirm
                    text: qsTr("This was me")
                    emphasized: false
                    accessibleDescription: qsTr(
                        "Tell QindaFirewall this login was you, so it stops warning about this address")
                    onClicked: {
                        access.confirmTopAlertMine()
                        details.close()
                    }
                }
            }
        }
    }

    ToolButton {
        id: summary
        objectName: "firewallAppletSummary"
        anchors.fill: parent
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Firewall: %1").arg(
            root.available ? access.stateText : qsTr("Background watching is off. Open QindaFirewall to turn it on."))
        ToolTip.visible: hovered
        ToolTip.text: root.available ? access.stateText
                                     : qsTr("Background watching is off. Open QindaFirewall to turn it on.")

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
            implicitWidth: glyph.implicitWidth
            implicitHeight: glyph.implicitHeight

            // AGENT-NOTE: Alarm's persistent treatment is a fixed ring, not
            // an animation, so it stays visible under reduced-motion and
            // never competes with the badge for the same corner.
            Rectangle {
                id: alarmRing
                visible: root.persistent
                anchors.centerIn: glyph
                width: glyph.width + 10
                height: glyph.height + 10
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: Tokens.danger.default
                Accessible.ignored: true
            }

            ShellIcons.Icon {
                id: glyph
                objectName: "firewallAppletIcon"
                anchors.centerIn: parent
                name: "security-high-symbolic"
                size: Math.min(20, root.height - 8)
                color: root.iconColor
                symbolic: true
                fallbackText: qsTr("Firewall")
                Accessible.ignored: true
            }

            Rectangle {
                id: badge
                objectName: "firewallAppletBadge"
                visible: root.badge
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
                    text: root.alertCount > 99 ? qsTr("99+") : String(root.alertCount)
                    color: Tokens.danger.fg
                    font.pointSize: Tokens.type.caption
                    font.bold: true
                }
            }
        }
        background: Item {}
    }
}
