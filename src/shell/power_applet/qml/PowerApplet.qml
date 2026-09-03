// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
    implicitWidth: vertical ? 40 : Math.max(46, summary.implicitWidth + 12)
    implicitHeight: vertical ? 40 : 28

    ToolButton {
        id: summary
        objectName: "powerAppletSummary"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        text: root.access !== null ? root.access.batteryLabel : qsTr("Power")
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

        contentItem: Text {
            text: summary.text
            color: root.colors.text ?? "white"
            font.pixelSize: root.vertical ? 10 : 11
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
        }
        background: Item {}
    }

    Popup {
        id: details
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

                Button {
                    required property var modelData

                    objectName: "powerAppletProfileButton"
                    Layout.fillWidth: true
                    text: modelData.active
                          ? qsTr("%1 (current)").arg(modelData.label)
                          : modelData.label
                    enabled: modelData.adjustable && !modelData.pending
                             && !root.access.operationPending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.accessibleName
                    Accessible.description: modelData.accessibleDescription
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

                Button {
                    objectName: "powerAppletLockButton"
                    Layout.fillWidth: true
                    text: qsTr("Lock")
                    enabled: root.sessionActions !== null
                             && root.sessionActions.canLock
                             && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Lock session")
                    Accessible.description: qsTr("Lock the current QindaQt session")
                    onClicked: root.sessionActions.requestLock()
                }
                Button {
                    objectName: "powerAppletLogoutButton"
                    Layout.fillWidth: true
                    text: qsTr("Log out")
                    enabled: root.sessionActions !== null
                             && root.sessionActions.canLogout
                             && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.description: qsTr("Confirm and end the current QindaQt session")
                    onClicked: {
                        root.confirmationAction = "logout"
                        confirmation.open()
                    }
                }
                Button {
                    objectName: "powerAppletSuspendButton"
                    Layout.fillWidth: true
                    text: qsTr("Suspend")
                    enabled: root.sessionActions !== null
                             && root.sessionActions.canSuspend
                             && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.description: qsTr("Suspend the computer now")
                    onClicked: root.sessionActions.requestSuspend()
                }
                Button {
                    objectName: "powerAppletRestartButton"
                    Layout.fillWidth: true
                    text: qsTr("Restart")
                    enabled: root.sessionActions !== null
                             && root.sessionActions.canReboot
                             && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.description: qsTr("Confirm and restart the computer")
                    onClicked: {
                        root.confirmationAction = "reboot"
                        confirmation.open()
                    }
                }
                Button {
                    objectName: "powerAppletPowerOffButton"
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    text: qsTr("Shut down")
                    enabled: root.sessionActions !== null
                             && root.sessionActions.canPowerOff
                             && !root.sessionActions.pending
                    focusPolicy: Qt.TabFocus
                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.description: qsTr("Confirm and shut down the computer")
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
