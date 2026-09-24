// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

T.Page {
    id: root
    required property var quietingSettings
    // ADR-0212: the Do Not Disturb schedule, over the same purpose-scoped
    // Settings1 client the switch uses.
    required property var quietingSchedule
    required property var applicationPolicies
    readonly property Item firstFocusTarget: doNotDisturbSwitch
    // The ring's two ends. Only one action is ever projected at a time, but
    // forward and back keep their original preference so the order cannot
    // change under a state that shows both.
    readonly property Item actionAfterApplications: conflictAction.visible
                                                    ? conflictAction
                                                    : retryAction.visible
                                                    ? retryAction : doNotDisturbSwitch
    readonly property Item forwardAction: applicationPoliciesSection.firstControl
                                          ?? actionAfterApplications
    readonly property Item backwardAction: retryAction.visible
                                           ? retryAction
                                           : conflictAction.visible
                                           ? conflictAction
                                           : applicationPoliciesSection.lastControl
                                           ?? quietHours.lastControl
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
            // AGENT-GUARD: this page's focus ring is written out by hand so
            // every control stays reachable. Do Not Disturb hands forward to
            // the schedule, the schedule's last field hands forward to
            // whichever action is visible, and that action closes the ring.
            KeyNavigation.tab: quietHours.firstControl
            KeyNavigation.backtab: root.backwardAction
            Accessible.role: Accessible.CheckBox
            Accessible.name: qsTr("Do Not Disturb")
            Accessible.description: qsTr(
                "Low and normal banners are hidden; critical banners bypass Do Not Disturb unless their app is muted, and lock privacy always applies")
            Accessible.checked: checked
            onClicked: {
                root.quietingSettings.requestSet(
                    !root.quietingSettings.enabled)
                // Qt's Switch writes checked before clicked, detaching the
                // truth binding even when the request is refused immediately.
                Qt.callLater(() => {
                    doNotDisturbSwitch.checked = Qt.binding(
                        () => root.quietingSettings.enabled)
                })
            }
        }

        QuietHoursSection {
            id: quietHours
            Layout.fillWidth: true
            schedule: root.quietingSchedule
            focusBefore: doNotDisturbSwitch
            focusAfter: root.forwardAction
        }

        NotificationApplicationPoliciesSection {
            id: applicationPoliciesSection
            Layout.fillWidth: true
            policies: root.applicationPolicies
            focusBefore: quietHours.lastControl
            focusAfter: root.actionAfterApplications
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
                KeyNavigation.backtab: applicationPoliciesSection.lastControl
                                          ?? quietHours.lastControl
                KeyNavigation.tab: doNotDisturbSwitch
                onClicked: root.quietingSettings.applyMyChoice()
            }

            Button {
                id: retryAction
                objectName: "settingsRetryButton"
                visible: root.quietingSettings.unavailable
                text: qsTr("Retry")
                focusPolicy: Qt.StrongFocus
                KeyNavigation.backtab: applicationPoliciesSection.lastControl
                                          ?? quietHours.lastControl
                KeyNavigation.tab: doNotDisturbSwitch
                onClicked: root.quietingSettings.retry()
            }

        }
    }
}
