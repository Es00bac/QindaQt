// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    required property var quietingSettings
    signal closeRequested()
    readonly property Item firstFocusTarget: doNotDisturbSwitch
    title: qsTr("Notifications")

    Component.onCompleted: doNotDisturbSwitch.forceActiveFocus(Qt.TabFocusReason)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Label {
            objectName: "notificationsPageHeading"
            text: qsTr("Notifications")
            font.pixelSize: 24
            font.bold: true
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
                               : retryAction.visible ? retryAction : closeButton
            KeyNavigation.backtab: closeButton
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
                KeyNavigation.tab: closeButton
                KeyNavigation.backtab: doNotDisturbSwitch
                onClicked: root.quietingSettings.applyMyChoice()
            }

            Button {
                id: retryAction
                objectName: "settingsRetryButton"
                visible: root.quietingSettings.unavailable
                text: qsTr("Retry")
                focusPolicy: Qt.StrongFocus
                KeyNavigation.tab: closeButton
                KeyNavigation.backtab: doNotDisturbSwitch
                onClicked: root.quietingSettings.retry()
            }

            Button {
                id: closeButton
                objectName: "settingsCloseButton"
                text: qsTr("Close")
                focusPolicy: Qt.StrongFocus
                KeyNavigation.tab: doNotDisturbSwitch
                KeyNavigation.backtab: conflictAction.visible
                                       ? conflictAction
                                       : retryAction.visible ? retryAction : doNotDisturbSwitch
                onClicked: root.closeRequested()
            }
        }
    }
}
