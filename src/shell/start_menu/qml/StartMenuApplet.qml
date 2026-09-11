// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Panel button of the start-menu applet (ADR-0124): the worn-Luna start
// button hosting the two-column start panel. This module is pure
// presentation over the facades the panel dispatcher already owns; it keeps
// no state and starts nothing itself.
//
// AGENT-CONTRACT: both facades are borrowed and may be null (preview,
// degraded composition, grant denied — the same fail-closed rule as
// LauncherAppletController and DesktopControlsAccess). Without them the
// button renders the identical visuals at half opacity and refuses to open;
// nothing may dereference an access before `available` is true.
//
// AGENT-NOTE: every #rrggbb value in this module is a Luna dressing constant
// of the Bliss start-menu experience (ADR-0124), the same instance-level
// exception as the `luna` path in TaskListEntryButton. They are presentation
// constants of this dressing, never theme authority: content controls that
// carry their own surfaces (the search field) keep token styling.
Item {
    id: root

    property var applet: null
    property var theme: null
    property bool vertical: false
    required property var launcherAppletAccess
    required property var desktopControlsAccess

    // AGENT-GUARD: Tokens.ready gates every access read, exactly like
    // LauncherApplet. Dropping it dereferences facades before the engine has
    // published QST-1 and crashes preview composition.
    readonly property bool available: launcherAppletAccess !== null
        && desktopControlsAccess !== null && Tokens.ready

    objectName: "startMenuApplet"
    // Manifest sizing: mainAxis preferred 56, crossAxis preferred 36.
    implicitWidth: 56
    implicitHeight: 36
    opacity: available ? 1.0 : 0.5

    function openPanel() {
        if (!root.available)
            return
        popup.open()
    }

    // Session truth is resolved once here and shared with the popup; both
    // fail closed to null when the facade (or its sub-object) is absent.
    readonly property var session: desktopControlsAccess !== null
        && desktopControlsAccess.systemMenu !== null
        && desktopControlsAccess.systemMenu.sessionActionsAvailable
        ? desktopControlsAccess.systemMenu.sessionActions : null

    StartMenuPopup {
        id: popup
        launcher: root.launcherAppletAccess
        controls: root.desktopControlsAccess
        onLogOffRequested: {
            popup.close()
            logOffConfirmation.open()
        }
        onClosed: button.forceActiveFocus(Qt.PopupFocusReason)
    }

    // Same confirmation rule as the system menu's session actions (ADR-0070):
    // logout never dispatches without this dialog, and the dialog is a
    // sibling of the popup — never nested inside it.
    T.Dialog {
        id: logOffConfirmation

        objectName: "startMenuLogOffConfirmation"
        popupType: T.Popup.Window
        width: 320
        modal: true
        focus: true
        title: qsTr("Log off?")
        standardButtons: T.Dialog.Cancel | T.Dialog.Ok
        onAccepted: if (root.session !== null)
            root.session.requestLogout()
        contentItem: C.Label {
            text: qsTr("Open applications will be asked to close"
                       + " before this session ends.")
            Accessible.role: Accessible.Dialog
            Accessible.name: logOffConfirmation.title
            Accessible.description: text
        }
    }

    T.ToolButton {
        id: button

        objectName: "startMenuButton"
        anchors.fill: parent
        enabled: root.available
        focusPolicy: Qt.TabFocus
        hoverEnabled: true
        Accessible.role: Accessible.Button
        Accessible.name: root.available ? qsTr("Start")
                                        : qsTr("Start menu is unavailable")
        Accessible.description: root.available
                                ? qsTr("Opens the start panel") : ""
        onClicked: root.openPanel()
        Keys.onReturnPressed: root.openPanel()
        Keys.onEnterPressed: root.openPanel()
        Accessible.onPressAction: root.openPanel()

        contentItem: Row {
            spacing: 6
            anchors.centerIn: parent

            ShellIcons.Icon {
                objectName: "startMenuButtonIcon"
                anchors.verticalCenter: parent.verticalCenter
                name: "start-here-kde"
                size: 20
                color: "#ffffff"
                fallbackText: qsTr("Start")
                Accessible.ignored: true
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("start")
                color: "#ffffff"
                font.family: "Trebuchet MS"
                font.pointSize: 11
                font.bold: true
                font.italic: false
                Accessible.ignored: true
            }
        }
        background: Rectangle {
            objectName: "startMenuButtonChrome"
            radius: 4
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: button.down ? "#2f7d2c"
                         : button.hovered ? "#4dab49" : "#3d9a3a"
                }
                GradientStop {
                    position: 1.0
                    color: button.down ? "#215e1f"
                         : button.hovered ? "#3a8c37" : "#2b7a29"
                }
            }

            // Worn-Luna bottom edge: one dark scanline separating the button
            // from the panel surface beneath it.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#1d5c1c"
                Accessible.ignored: true
            }

            C.FocusRing {
                anchors.fill: parent
                control: button
            }
        }
    }
}
