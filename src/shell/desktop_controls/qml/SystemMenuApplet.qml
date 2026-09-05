// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

// System menu (macOS- and MATE-inspired presets): About, System Settings, and
// the session actions. Settings opens through the launcher facade; session
// requests re-enter the shell-owned session-actions facade with the same
// confirmation rule the Power applet applies to logout/restart/shut down.
Item {
    id: root

    required property var access
    property bool vertical: false
    property string confirmationAction: ""

    readonly property bool ready: access !== null && Tokens.ready
    readonly property var session: ready ? access.sessionActions : null
    readonly property bool sessionReady: session !== null && session !== undefined

    objectName: "systemMenuApplet"
    implicitWidth: button.implicitWidth
    implicitHeight: 28

    function sessionEnabled(capability) {
        return root.sessionReady && Boolean(root.session[capability]) && !root.session.pending
    }

    function confirm(action) {
        root.confirmationAction = action
        menu.close()
        confirmation.open()
    }

    DesktopControlButton {
        id: button
        objectName: "systemMenuButton"
        anchors.fill: parent
        iconName: "preferences-system"
        fallbackText: qsTr("System")
        vertical: root.vertical
        available: root.ready
        active: menu.opened
        Accessible.name: qsTr("System menu")
        accessibleDescription: available
                               ? qsTr("About, System Settings, and session actions")
                               : qsTr("The system menu is unavailable")
        onTriggered: if (available) menu.open()
    }

    ControlPopupFrame {
        id: menu
        objectName: "systemMenuPopup"
        heading: root.ready ? String(root.access.productName) : qsTr("QindaQt")
        feedback: root.ready && root.access.feedbackPresent ? String(root.access.feedback)
                  : root.sessionReady && root.session.feedback.length > 0 ? String(root.session.feedback) : ""
        initialFocusItem: aboutRow
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)

        MenuRow {
            id: aboutRow
            objectName: "systemMenuAbout"
            Layout.fillWidth: true
            iconName: "help-about"
            text: qsTr("About QindaQt")
            detail: root.ready ? qsTr("Version %1").arg(String(root.access.versionText)) : ""
            KeyNavigation.down: settingsRow
            onActivated: { menu.close(); about.open() }
        }

        MenuRow {
            id: settingsRow
            objectName: "systemMenuSettings"
            Layout.fillWidth: true
            iconName: "preferences-system"
            text: qsTr("System Settings")
            detail: qsTr("Opens the QindaQt Settings application")
            enabled: root.ready && Boolean(root.access.canOpenSettings)
            KeyNavigation.up: aboutRow
            KeyNavigation.down: lockRow
            onActivated: if (root.access.openSettings()) menu.close()
        }

        MenuRow {
            id: lockRow
            objectName: "systemMenuLock"
            Layout.fillWidth: true
            iconName: "system-lock-screen"
            text: qsTr("Lock")
            detail: qsTr("Lock the current QindaQt session")
            enabled: root.sessionEnabled("canLock")
            KeyNavigation.up: settingsRow
            KeyNavigation.down: logoutRow
            onActivated: { root.session.requestLock(); menu.close() }
        }

        MenuRow {
            id: logoutRow
            objectName: "systemMenuLogout"
            Layout.fillWidth: true
            iconName: "system-log-out"
            text: qsTr("Log out…")
            detail: qsTr("Confirm and end the current session")
            enabled: root.sessionEnabled("canLogout")
            KeyNavigation.up: lockRow
            KeyNavigation.down: suspendRow
            onActivated: root.confirm("logout")
        }

        MenuRow {
            id: suspendRow
            objectName: "systemMenuSuspend"
            Layout.fillWidth: true
            iconName: "system-suspend"
            text: qsTr("Suspend")
            detail: qsTr("Suspend the computer now")
            enabled: root.sessionEnabled("canSuspend")
            KeyNavigation.up: logoutRow
            KeyNavigation.down: restartRow
            onActivated: { root.session.requestSuspend(); menu.close() }
        }

        MenuRow {
            id: restartRow
            objectName: "systemMenuRestart"
            Layout.fillWidth: true
            iconName: "system-reboot"
            text: qsTr("Restart…")
            detail: qsTr("Confirm and restart the computer")
            enabled: root.sessionEnabled("canReboot")
            KeyNavigation.up: suspendRow
            KeyNavigation.down: powerOffRow
            onActivated: root.confirm("reboot")
        }

        MenuRow {
            id: powerOffRow
            objectName: "systemMenuPowerOff"
            Layout.fillWidth: true
            iconName: "system-shutdown"
            text: qsTr("Shut down…")
            detail: qsTr("Confirm and shut down the computer")
            destructive: true
            enabled: root.sessionEnabled("canPowerOff")
            KeyNavigation.up: restartRow
            onActivated: root.confirm("poweroff")
        }
    }

    ControlPopupFrame {
        id: about
        objectName: "systemMenuAboutPopup"
        heading: qsTr("About QindaQt")
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)

        C.Label {
            Layout.fillWidth: true
            objectName: "systemMenuAboutText"
            text: qsTr("QindaQt %1 — a modular Qt desktop with hybrid window containers.")
                  .arg(root.ready ? String(root.access.versionText) : "")
        }
    }

    T.Dialog {
        id: confirmation
        objectName: "systemMenuConfirmation"
        popupType: T.Popup.Window
        width: 320
        modal: true
        focus: true
        title: root.confirmationAction === "logout" ? qsTr("Log out?")
               : root.confirmationAction === "reboot" ? qsTr("Restart?") : qsTr("Shut down?")
        standardButtons: T.Dialog.Cancel | T.Dialog.Ok
        onAccepted: {
            if (!root.sessionReady)
                return
            if (root.confirmationAction === "logout")
                root.session.requestLogout()
            else if (root.confirmationAction === "reboot")
                root.session.requestReboot()
            else if (root.confirmationAction === "poweroff")
                root.session.requestPowerOff()
        }
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)
        contentItem: C.Label {
            text: root.confirmationAction === "logout"
                  ? qsTr("Open applications will be asked to close before this session ends.")
                  : root.confirmationAction === "reboot"
                    ? qsTr("The computer will restart now.")
                    : qsTr("The computer will shut down now.")
            Accessible.role: Accessible.Dialog
            Accessible.name: confirmation.title
            Accessible.description: text
        }
    }
}
