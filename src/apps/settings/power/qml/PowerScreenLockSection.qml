// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var screenLockSettings
    readonly property Item firstActionTarget: automaticLock.enabled ? automaticLock : null

    Layout.fillWidth: true
    spacing: Tokens.space["2"]
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Screen lock")

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Screen lock")
        description: qsTr("Choose whether inactivity locks this session")
    }

    FormSurface {
        Layout.fillWidth: true
        padding: Tokens.space["3"]
        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Automatic screen lock")

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            Switch {
                id: automaticLock
                objectName: "powerAutomaticScreenLock"
                Layout.fillWidth: true
                text: qsTr("Lock automatically when idle")
                checked: root.screenLockSettings.automaticLock
                enabled: !root.screenLockSettings.busy
                accessibleDescription: checked
                    ? qsTr("The screen locks after the selected idle time")
                    : qsTr("The screen stays unlocked until you lock it yourself")
                onToggled: root.screenLockSettings.setAutomaticLock(checked)
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: root.screenLockSettings.automaticLock
                         && !root.screenLockSettings.busy
                spacing: Tokens.space["2"]

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Lock after %1 minutes").arg(root.screenLockSettings.timeoutMinutes)
                    Accessible.name: text
                    muted: !parent.enabled
                }
                Button {
                    objectName: "powerScreenLockTimeoutDecrease"
                    text: qsTr("Less")
                    available: parent.enabled
                               && root.screenLockSettings.timeoutMinutes > 1
                    accessibleDescription: qsTr("Reduce automatic lock timeout by one minute")
                    onClicked: root.screenLockSettings.setTimeoutMinutes(
                                   root.screenLockSettings.timeoutMinutes - 1)
                }
                Button {
                    objectName: "powerScreenLockTimeoutIncrease"
                    text: qsTr("More")
                    available: parent.enabled
                               && root.screenLockSettings.timeoutMinutes < 240
                    accessibleDescription: qsTr("Increase automatic lock timeout by one minute")
                    onClicked: root.screenLockSettings.setTimeoutMinutes(
                                   root.screenLockSettings.timeoutMinutes + 1)
                }
            }

            Label {
                objectName: "powerScreenLockStatus"
                Layout.fillWidth: true
                text: root.screenLockSettings.statusText
                wrapMode: Text.Wrap
                muted: true
                Accessible.name: text
            }
            Label {
                objectName: "powerScreenLockError"
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.screenLockSettings.errorText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
            Button {
                objectName: "powerScreenLockRetry"
                visible: root.screenLockSettings.errorText.length > 0
                text: qsTr("Try again")
                available: !root.screenLockSettings.busy
                accessibleDescription: qsTr("Retry the failed screen-lock settings step")
                onClicked: root.screenLockSettings.retryLiveApply()
            }
        }
    }
}
