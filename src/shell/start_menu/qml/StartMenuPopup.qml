// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// Two-column Luna start panel (ADR-0124): applications on the left through
// the launcher facade, places and the session footer on the right through
// the desktop-controls sub-facades. Pure presentation: activation, folder
// opening, and session dispatch all re-enter the facades' own bounded seams.
//
// AGENT-GUARD: keep popupType Window. The layer-shell panel rejects keyboard
// focus and cannot paint outside its surface, so like every outward-facing
// shell surface (LauncherApplet, ControlPopupFrame) this panel must be its
// own window and seed its initial focus itself.
//
// AGENT-NOTE: the footer log-off re-enters the session-actions facade that
// SystemMenuController already projects (ADR-0070), gated by canLogout and
// pending, with the same confirmation rule the system menu applies. It never
// calls a session API the facade does not publish; without the facade the
// button renders disabled. The popup only emits logOffRequested — the
// confirmation dialog lives beside this popup in StartMenuApplet, because a
// dialog nested inside a popup closes together with its parent popup.
T.Popup {
    id: popup

    required property var launcher
    required property var controls

    readonly property bool launcherReady: launcher !== null && Tokens.ready
    readonly property bool controlsReady: controls !== null && Tokens.ready
    // The system-menu sub-facade is itself optional inside the access root.
    readonly property var sessionMenu: controlsReady
        ? controls.systemMenu ?? null : null
    readonly property var session: sessionMenu !== null
        && sessionMenu.sessionActionsAvailable ? sessionMenu.sessionActions
                                               : null

    function sessionEnabled(capability) {
        return popup.session !== null && Boolean(popup.session[capability])
               && !popup.session.pending
    }

    // The confirmation UX is owned by StartMenuApplet; see the AGENT-NOTE.
    signal logOffRequested()

    objectName: "startMenuPopup"
    popupType: T.Popup.Window
    width: 380
    height: Math.min(480, panelColumn.implicitHeight + 2)
    padding: 0
    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onOpened: Qt.callLater(function() {
        if (popup.launcherReady)
            leftColumn.focusSearch()
    })

    background: Rectangle {
        objectName: "startMenuChrome"
        radius: 3
        color: "#ffffff"
        border.width: 1
        border.color: "#7a6a54"
    }

    contentItem: ColumnLayout {
        id: panelColumn

        spacing: 0

        Rectangle {
            objectName: "startMenuHeader"
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: 3
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1b47a4" }
                GradientStop { position: 1.0; color: "#2e68d6" }
            }

            // Square the band's lower corners under the rounded chrome.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height / 2
                color: "#2e68d6"
                Accessible.ignored: true
            }

            C.Label {
                objectName: "startMenuHeading"
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("QindaQt")
                color: "#ffffff"
                font.family: "Trebuchet MS"
                font.pointSize: 12
                font.bold: true
                Accessible.role: Accessible.Heading
            }
        }

        RowLayout {
            id: bodyRow

            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            StartMenuLeftColumn {
                id: leftColumn

                Layout.fillWidth: true
                Layout.fillHeight: true
                launcher: popup.launcher
            }

            StartMenuRightColumn {
                id: rightColumn

                Layout.preferredWidth: 130
                Layout.fillHeight: true
                places: popup.controlsReady
                        ? popup.controls.places ?? null : null
                onPlaceOpened: popup.close()
            }
        }

        Rectangle {
            objectName: "startMenuFooter"
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: 3
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1b47a4" }
                GradientStop { position: 1.0; color: "#2e68d6" }
            }

            // Square the band's upper corners under the rounded chrome.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: parent.height / 2
                color: "#1b47a4"
                Accessible.ignored: true
            }

            T.ToolButton {
                id: logOffButton

                objectName: "startMenuLogOffButton"
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                enabled: popup.sessionEnabled("canLogout")
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Log Off")
                Accessible.description:
                    qsTr("Confirm and end the current session")

                function requestLogOff() {
                    popup.logOffRequested()
                }

                onClicked: requestLogOff()
                Keys.onReturnPressed: requestLogOff()
                Keys.onEnterPressed: requestLogOff()
                Accessible.onPressAction: requestLogOff()

                contentItem: Row {
                    spacing: 4
                    anchors.centerIn: parent

                    ShellIcons.Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "system-log-out"
                        size: 16
                        color: "#ffffff"
                        fallbackText: qsTr("Log Off")
                        Accessible.ignored: true
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Log Off")
                        color: logOffButton.enabled ? "#ffffff" : "#b7c8ea"
                        font.family: "Trebuchet MS"
                        font.pointSize: 10
                        font.bold: true
                        Accessible.ignored: true
                    }
                }
                background: Item {}
            }
        }
    }
}
