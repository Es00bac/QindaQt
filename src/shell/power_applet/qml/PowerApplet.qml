// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

Item {
    id: root

    required property var access
    required property var theme
    property bool vertical: false
    readonly property var colors: theme.colors ?? ({})
    readonly property bool available: access !== null
    readonly property var sessionActions: root.available
                                          ? root.access.sessionActions : null
    property string confirmationAction: ""

    objectName: "powerApplet"
    readonly property string percentageText:
        available && /[0-9]+%/.test(access.batteryLabel)
        ? access.batteryLabel.match(/[0-9]+%/)[0] : ""
    readonly property string batteryIconName: {
        if (percentageText === "")
            return "battery-missing"
        const level = Number(percentageText.slice(0, -1))
        if (level >= 90) return "battery-100"
        if (level >= 70) return "battery-080"
        if (level >= 50) return "battery-060"
        if (level >= 30) return "battery-040"
        if (level >= 10) return "battery-020"
        return "battery-000"
    }

    implicitWidth: vertical ? 32 : (percentageText === "" ? 32 : 62)
    implicitHeight: 28

    ToolButton {
        id: summary
        objectName: "powerAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: ""
        Accessible.role: Accessible.Button
        Accessible.name: root.access !== null
                         ? root.access.accessibleName
                         : qsTr("Power information is unavailable")
        Accessible.description: root.access !== null
                                ? root.access.accessibleDescription : ""

        function openDetails() {
            if (root.available)
                details.open()
        }

        onClicked: openDetails()
        Accessible.onPressAction: openDetails()

        contentItem: RowLayout {
            spacing: 4

            ShellIcons.Icon {
                objectName: "powerAppletIcon"
                name: root.batteryIconName
                size: Math.min(20, root.height - 8)
                color: Tokens.fg.default
                symbolic: true
                fallbackText: qsTr("Battery")
                Accessible.ignored: true
            }

            Text {
                objectName: "powerAppletPercentage"
                visible: !root.vertical && root.percentageText !== ""
                text: root.percentageText
                color: Tokens.fg.default
                font.pixelSize: 11
                textFormat: Text.PlainText
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
        objectName: "powerAppletPopup"
        width: 300
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: root.theme.cornerRadius ?? 10
            color: root.colors.surfaceRaised ?? "#2c312e"
            border.color: root.colors.border ?? "#3c433f"
        }

        contentItem: ColumnLayout {
            spacing: 8

            Label {
                objectName: "powerAppletHeading"
                Layout.fillWidth: true
                text: root.access !== null ? root.access.accessibleName
                                           : qsTr("Power unavailable")
                color: root.colors.text ?? "white"
                wrapMode: Text.Wrap
                Accessible.role: Accessible.Heading
            }

            Label {
                objectName: "powerAppletLoading"
                Layout.fillWidth: true
                visible: root.access !== null && root.access.phase === "loading"
                text: qsTr("Power information is loading…")
                color: root.colors.textMuted ?? "#a9afa9"
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                visible: root.access !== null && root.access.diagnostic !== ""
                text: visible ? root.access.diagnostic : ""
                color: root.colors.textMuted ?? "#a9afa9"
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                visible: root.access !== null && root.access.profileRows.length > 0
                text: qsTr("Power profile")
                color: root.colors.text ?? "white"
                font.bold: true
            }

            Repeater {
                model: root.access !== null ? root.access.profileRows : []

                C.Button {
                    required property var modelData

                    objectName: "powerAppletProfileButton"
                    Layout.fillWidth: true
                    emphasized: modelData.active
                    text: modelData.active
                          ? qsTr("%1 (current)").arg(modelData.label)
                          : modelData.label
                    available: modelData.adjustable && !modelData.pending
                               && !root.access.operationPending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.accessibleName
                    accessibleDescription: modelData.accessibleDescription
                    Accessible.checked: modelData.active
                    onClicked: root.access.requestProfile(modelData.profileId)
                    Accessible.onPressAction: {
                        if (enabled)
                            root.access.requestProfile(modelData.profileId)
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.access !== null && root.access.keyboardRows.length > 0
                text: qsTr("Keyboard brightness")
                color: root.colors.text ?? "white"
                font.bold: true
            }

            Repeater {
                model: root.access !== null ? root.access.keyboardRows : []

                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: parent.modelData.accessibleName
                        color: root.colors.text ?? "white"
                        elide: Text.ElideRight
                    }

                    Slider {
                        objectName: "powerAppletKeyboardSlider"
                        Layout.fillWidth: true
                        from: 0
                        to: 10000
                        stepSize: 100
                        value: parent.modelData.currentKnown
                               ? parent.modelData.normalizedCurrent : 0
                        enabled: parent.modelData.adjustable
                                 && !parent.modelData.pending
                                 && !root.access.operationPending
                        focusPolicy: Qt.TabFocus
                        Accessible.name: parent.modelData.accessibleName
                        Accessible.description:
                            parent.modelData.accessibleDescription
                        onMoved: root.access.requestKeyboardBrightness(
                                     parent.modelData.controlId,
                                     Math.round(value))
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Session")
                color: root.colors.text ?? "white"
                font.bold: true
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2

                C.Button {
                    objectName: "powerAppletLockButton"
                    Layout.fillWidth: true
                    emphasized: false
                    text: qsTr("Lock")
                    available: root.sessionActions !== null
                               && root.sessionActions.canLock
                               && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Lock session")
                    accessibleDescription: qsTr("Lock the current QindaQt session")
                    onClicked: root.sessionActions.requestLock()
                }
                C.Button {
                    objectName: "powerAppletLogoutButton"
                    Layout.fillWidth: true
                    emphasized: false
                    text: qsTr("Log out")
                    available: root.sessionActions !== null
                               && root.sessionActions.canLogout
                               && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    accessibleDescription: qsTr("Confirm and end the current QindaQt session")
                    onClicked: {
                        root.confirmationAction = "logout"
                        confirmation.open()
                    }
                }
                C.Button {
                    objectName: "powerAppletSuspendButton"
                    Layout.fillWidth: true
                    emphasized: false
                    text: qsTr("Suspend")
                    available: root.sessionActions !== null
                               && root.sessionActions.canSuspend
                               && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    accessibleDescription: qsTr("Suspend the computer now")
                    onClicked: root.sessionActions.requestSuspend()
                }
                C.Button {
                    objectName: "powerAppletRestartButton"
                    Layout.fillWidth: true
                    emphasized: false
                    text: qsTr("Restart")
                    available: root.sessionActions !== null
                               && root.sessionActions.canReboot
                               && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    accessibleDescription: qsTr("Confirm and restart the computer")
                    onClicked: {
                        root.confirmationAction = "reboot"
                        confirmation.open()
                    }
                }
                C.Button {
                    objectName: "powerAppletPowerOffButton"
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    emphasized: false
                    text: qsTr("Shut down")
                    available: root.sessionActions !== null
                               && root.sessionActions.canPowerOff
                               && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    accessibleDescription: qsTr("Confirm and shut down the computer")
                    onClicked: {
                        root.confirmationAction = "poweroff"
                        confirmation.open()
                    }
                }
            }

            Label {
                objectName: "powerAppletSessionFeedback"
                Layout.fillWidth: true
                visible: root.sessionActions !== null
                         && root.sessionActions.feedback !== ""
                text: visible ? root.sessionActions.feedback : ""
                color: root.colors.warning ?? "#e5a84b"
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            Label {
                objectName: "powerAppletFeedback"
                Layout.fillWidth: true
                visible: root.access !== null && root.access.feedbackPresent
                text: visible ? root.access.feedback : ""
                color: root.colors.warning ?? "#e5a84b"
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
            }
        }
    }

    Dialog {
        popupType: Popup.Window
        id: confirmation
        objectName: "powerAppletSessionConfirmation"
        width: 320
        modal: true
        focus: true
        title: root.confirmationAction === "logout" ? qsTr("Log out?")
               : root.confirmationAction === "reboot" ? qsTr("Restart?")
               : qsTr("Shut down?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            if (root.sessionActions === null)
                return
            if (root.confirmationAction === "logout")
                root.sessionActions.requestLogout()
            else if (root.confirmationAction === "reboot")
                root.sessionActions.requestReboot()
            else if (root.confirmationAction === "poweroff")
                root.sessionActions.requestPowerOff()
        }
        contentItem: Label {
            text: root.confirmationAction === "logout"
                  ? qsTr("Open applications will be asked to close before this session ends.")
                  : root.confirmationAction === "reboot"
                    ? qsTr("The computer will restart now.")
                    : qsTr("The computer will shut down now.")
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Dialog
            Accessible.name: confirmation.title
            Accessible.description: text
        }
    }
}
