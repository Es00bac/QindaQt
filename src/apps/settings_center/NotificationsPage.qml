// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root
    required property var quietingSettings
    readonly property Item firstFocusTarget: doNotDisturbSwitch
    title: qsTr("Notifications")

    background: Rectangle { color: Tokens.bg.base }

    Component.onCompleted: doNotDisturbSwitch.forceActiveFocus(Qt.TabFocusReason)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "notificationsPageHeading"
            text: qsTr("Notifications")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Switch {
            id: doNotDisturbSwitch
            objectName: "settingsDoNotDisturbSwitch"
            text: qsTr("Do Not Disturb")
            checked: root.quietingSettings.enabled
            enabled: root.quietingSettings.canToggle
            focusPolicy: Qt.StrongFocus
            KeyNavigation.tab: conflictAction.visible
                               ? conflictAction
                               : retryAction.visible ? retryAction : doNotDisturbSwitch
            KeyNavigation.backtab: retryAction.visible
                                   ? retryAction
                                   : conflictAction.visible ? conflictAction : doNotDisturbSwitch
            Accessible.role: Accessible.CheckBox
            Accessible.name: qsTr("Do Not Disturb")
            Accessible.description: qsTr(
                "Low and normal notification banners are hidden; critical banners remain visible only when privacy permits")
            Accessible.checked: checked
            onClicked: root.quietingSettings.requestSet(
                           !root.quietingSettings.enabled)
        }

        Label {
            objectName: "settingsQuietingStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.quietingSettings.statusText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: root.quietingSettings.conflict
                             || root.quietingSettings.unavailable
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "settingsQuietingError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.quietingSettings.errorText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                id: conflictAction
                objectName: "settingsConflictApplyButton"
                visible: root.quietingSettings.conflict
                text: qsTr("Apply my choice")
                focusPolicy: Qt.StrongFocus
            KeyNavigation.backtab: doNotDisturbSwitch
            KeyNavigation.tab: doNotDisturbSwitch
                onClicked: root.quietingSettings.applyMyChoice()
            }

            Button {
                id: retryAction
                objectName: "settingsRetryButton"
                visible: root.quietingSettings.unavailable
                text: qsTr("Retry")
                focusPolicy: Qt.StrongFocus
            KeyNavigation.backtab: doNotDisturbSwitch
            KeyNavigation.tab: doNotDisturbSwitch
                onClicked: root.quietingSettings.retry()
            }

        }
    }
}
