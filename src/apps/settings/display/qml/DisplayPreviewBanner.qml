// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Active transaction preview banner: provides live countdown feedback and
// direct access to Confirm and Revert transaction actions.
Rectangle {
    id: root

    required property var displaySettings

    visible: displaySettings.inTransaction
    implicitHeight: layout.implicitHeight + Tokens.space["4"] * 2
    radius: Tokens.radius.m
    color: displaySettings.awaitingConfirmation ? Tokens.status.warning.background
                                                : Tokens.status.info.background
    border.width: Tokens.space["1"] / 2
    border.color: displaySettings.awaitingConfirmation ? Tokens.status.warning.foreground
                                                       : Tokens.status.info.foreground

    Accessible.role: Accessible.AlertMessage
    Accessible.name: qsTr("Display configuration preview")
    Accessible.description: displaySettings.transactionStatusText

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Tokens.space["4"]
        spacing: Tokens.space["3"]

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["1"]

            Label {
                Layout.fillWidth: true
                text: displaySettings.awaitingConfirmation
                      ? qsTr("Previewing new display settings")
                      : qsTr("Applying display configuration")
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.body
                font.weight: Font.DemiBold
                color: displaySettings.awaitingConfirmation
                       ? Tokens.status.warning.foreground
                       : Tokens.status.info.foreground
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: displaySettings.transactionStatusText
                font.family: Tokens.type.fontFamily
                font.pointSize: Tokens.type.caption
                color: displaySettings.awaitingConfirmation
                       ? Tokens.status.warning.foreground
                       : Tokens.status.info.foreground
                wrapMode: Text.Wrap
            }
        }

        Button {
            id: revertButton
            objectName: "displayPreviewRevertButton"
            text: qsTr("Revert")
            available: displaySettings.inTransaction
            onClicked: displaySettings.revertTransaction()
        }

        Button {
            id: keepButton
            objectName: "displayPreviewKeepButton"
            text: qsTr("Keep Changes")
            emphasized: true
            available: displaySettings.awaitingConfirmation
            onClicked: displaySettings.confirmTransaction()
        }
    }
}
