// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0

ColumnLayout {
    id: root

    required property var sessionActions
    property string confirmationAction: ""
    readonly property Item firstActionTarget:
        lockButton.enabled ? lockButton
        : logoutButton.enabled ? logoutButton
        : suspendButton.enabled ? suspendButton
        : restartButton.enabled ? restartButton
        : powerOffButton.enabled ? powerOffButton : null

    Layout.fillWidth: true
    spacing: 8
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Session")

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Session")
        description: qsTr("Lock this session or request a machine power action")
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 2

            Button {
                id: lockButton
                objectName: "powerSessionLock"
                Layout.fillWidth: true
                text: qsTr("Lock")
                available: root.sessionActions !== null
                           && root.sessionActions.canLock
                busy: root.sessionActions !== null
                      && root.sessionActions.pending
                accessibleDescription: qsTr("Lock the current QindaQt session")
                onClicked: root.sessionActions.requestLock()
            }
            Button {
                id: logoutButton
                objectName: "powerSessionLogout"
                Layout.fillWidth: true
                text: qsTr("Log out")
                available: root.sessionActions !== null
                           && root.sessionActions.canLogout
                busy: root.sessionActions !== null
                      && root.sessionActions.pending
                accessibleDescription: qsTr("Confirm and end the current QindaQt session")
                onClicked: {
                    root.confirmationAction = "logout"
                    confirmation.open()
                }
            }
            Button {
                id: suspendButton
                objectName: "powerSessionSuspend"
                Layout.fillWidth: true
                text: qsTr("Suspend")
                available: root.sessionActions !== null
                           && root.sessionActions.canSuspend
                busy: root.sessionActions !== null
                      && root.sessionActions.pending
                accessibleDescription: qsTr("Suspend the computer now")
                onClicked: root.sessionActions.requestSuspend()
            }
            Button {
                id: restartButton
                objectName: "powerSessionRestart"
                Layout.fillWidth: true
                text: qsTr("Restart")
                available: root.sessionActions !== null
                           && root.sessionActions.canReboot
                busy: root.sessionActions !== null
                      && root.sessionActions.pending
                accessibleDescription: qsTr("Confirm and restart the computer")
                onClicked: {
                    root.confirmationAction = "reboot"
                    confirmation.open()
                }
            }
            Button {
                id: powerOffButton
                objectName: "powerSessionPowerOff"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                text: qsTr("Shut down")
                available: root.sessionActions !== null
                           && root.sessionActions.canPowerOff
                busy: root.sessionActions !== null
                      && root.sessionActions.pending
                accessibleDescription: qsTr("Confirm and shut down the computer")
                onClicked: {
                    root.confirmationAction = "poweroff"
                    confirmation.open()
                }
            }
        }

        Label {
            objectName: "powerSessionFeedback"
            Layout.fillWidth: true
            visible: root.sessionActions !== null
                     && root.sessionActions.feedback !== ""
            text: visible ? root.sessionActions.feedback : ""
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }

    T.Dialog {
        id: confirmation
        objectName: "powerSessionConfirmation"
        parent: T.Overlay.overlay
        width: 320
        anchors.centerIn: parent
        modal: true
        focus: true
        title: root.confirmationAction === "logout" ? qsTr("Log out?")
               : root.confirmationAction === "reboot" ? qsTr("Restart?")
               : qsTr("Shut down?")
        standardButtons: T.Dialog.Cancel | T.Dialog.Ok
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
