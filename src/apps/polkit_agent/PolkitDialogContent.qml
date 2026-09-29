// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0 as QQ
import QindaQt.Shell.Icons 1.0 as ShellIcons
import QindaQt.Tokens 1.0

// The authentication dialog's content, loaded both by Main.qml's layer-shell
// window and directly (by file path) by the offscreen QML test -- exactly
// how ColorPage.qml is loaded by both the Settings app and
// tst_color_page.cpp. This item owns no Wayland fact; every pixel here is
// ordinary Qt Quick.
Item {
    id: root

    required property var viewModel

    property bool showDetails: false

    anchors.fill: parent
    focus: true
    visible: Tokens.ready && viewModel.visible

    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Authentication required")

    Keys.onEscapePressed: function (event) {
        viewModel.cancel()
        event.accepted = true
    }

    Connections {
        target: viewModel
        function onAttemptFailed() {
            passwordField.clear()
            passwordField.forceActiveFocus()
        }
        function onFinished(gainedAuthorization) {
            passwordField.clear()
        }
        function onVisibleChanged() {
            if (viewModel.visible) {
                passwordField.forceActiveFocus()
            }
        }
    }

    // The scrim wears the same popup material as every other transient
    // overlay (GatherOverviewSurface.qml), so a theme's transparency choice
    // reaches it without inventing a second opacity number.
    Rectangle {
        objectName: "polkitAgentScrim"
        anchors.fill: parent
        color: Tokens.ready ? Tokens.bg.base : "transparent"
        opacity: !Tokens.ready ? 0
                 : Tokens.accessibility.reducedTransparency
                   || Tokens.accessibility.highContrast
                   ? 1
                   : Tokens.material.popup.opacity
    }

    Rectangle {
        id: card
        objectName: "polkitAgentCard"
        anchors.centerIn: parent
        // A ternary, not Math.min(): a call-shaped expression sitting
        // right before this Rectangle's first child item's "{" reads as
        // an unclosed function header to tools/check-source-shape's brace
        // scanner, which then treats everything up to the matching "}"
        // (the whole card) as that "function"'s body.
        width: parent.width - 48 < 440 ? parent.width - 48 : 440
        // AGENT-NOTE: a plain Rectangle does not bind height to
        // implicitHeight the way a Control does; this must be an explicit
        // height binding or the card collapses to zero height.
        height: column.implicitHeight + Tokens.space["6"] * 2
        radius: Tokens.radius.l
        color: Tokens.bg.raised
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.strong

        ColumnLayout {
            id: column
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Tokens.space["6"]
            spacing: Tokens.space["4"]

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["3"]

                ShellIcons.Icon {
                    objectName: "polkitAgentAppIcon"
                    name: viewModel.appIconName
                    size: 32
                    fallbackText: viewModel.appName
                    Accessible.ignored: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Text {
                        objectName: "polkitAgentTitle"
                        text: qsTr("Authentication required")
                        color: Tokens.fg.default
                        font.family: Tokens.type.fontFamily
                        font.pointSize: Tokens.type.title
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                    Text {
                        objectName: "polkitAgentAppName"
                        text: viewModel.appName
                        color: Tokens.fg.muted
                        font.family: Tokens.type.fontFamily
                        font.pointSize: Tokens.type.caption
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }
                }
            }

            Text {
                objectName: "polkitAgentMessage"
                Layout.fillWidth: true
                text: viewModel.message
                wrapMode: Text.WordWrap
                color: Tokens.fg.default
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            QQ.ComboBox {
                id: identityChooser
                objectName: "polkitAgentIdentityChooser"
                Layout.fillWidth: true
                visible: viewModel.identityChoiceVisible
                model: viewModel.identityLabels
                currentIndex: viewModel.selectedIdentityIndex
                accessibleDescription: qsTr("Choose which identity to authenticate as")
                onActivated: function (index) { viewModel.selectedIdentityIndex = index }
            }

            QQ.TextField {
                id: passwordField
                objectName: "polkitAgentResponseField"
                Layout.fillWidth: true
                // AGENT-GUARD: readOnly, not enabled/false, while busy. A
                // disabled item cannot hold or receive active focus, which
                // would silently swallow both Escape-to-cancel while polkit
                // is validating and the refocus after a failed attempt
                // (attemptFailed sets busy true in the same tick it asks
                // the field to refocus). readOnly blocks further typing
                // without leaving the focus chain.
                readOnly: viewModel.busy
                echoMode: viewModel.promptEchoAllowed ? TextInput.Normal : TextInput.Password
                accessibleName: viewModel.promptText.length > 0 ? viewModel.promptText
                                                                : qsTr("Password")
                onAccepted: viewModel.authenticate(text)
            }

            Text {
                objectName: "polkitAgentStatusLine"
                Layout.fillWidth: true
                text: viewModel.statusText
                visible: text.length > 0
                wrapMode: Text.WordWrap
                color: viewModel.statusIsError ? Tokens.danger.default : Tokens.fg.muted
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            QQ.Button {
                objectName: "polkitAgentDetailsToggle"
                text: root.showDetails ? qsTr("Hide details") : qsTr("Details")
                emphasized: false
                Layout.alignment: Qt.AlignLeft
                onClicked: root.showDetails = !root.showDetails
            }

            ColumnLayout {
                objectName: "polkitAgentDetails"
                Layout.fillWidth: true
                visible: root.showDetails
                spacing: Tokens.space["1"]

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Action: %1").arg(viewModel.actionId)
                    wrapMode: Text.WrapAnywhere
                    color: Tokens.fg.muted
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
                Text {
                    Layout.fillWidth: true
                    visible: viewModel.vendorName.length > 0
                    text: qsTr("Vendor: %1").arg(viewModel.vendorName)
                    wrapMode: Text.WrapAnywhere
                    color: Tokens.fg.muted
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
                Text {
                    Layout.fillWidth: true
                    visible: viewModel.programPath.length > 0
                    text: qsTr("Program: %1").arg(viewModel.programPath)
                    wrapMode: Text.WrapAnywhere
                    color: Tokens.fg.muted
                    font.family: Tokens.type.fontFamily
                    font.pointSize: Tokens.type.caption
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                spacing: Tokens.space["3"]

                Item { Layout.fillWidth: true }

                QQ.Button {
                    objectName: "polkitAgentCancelButton"
                    text: qsTr("Cancel")
                    emphasized: false
                    onClicked: viewModel.cancel()
                }
                QQ.Button {
                    objectName: "polkitAgentAuthenticateButton"
                    text: qsTr("Authenticate")
                    emphasized: true
                    available: !viewModel.busy
                    onClicked: viewModel.authenticate(passwordField.text)
                }
            }
        }
    }
}
