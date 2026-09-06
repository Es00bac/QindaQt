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
    readonly property var timeoutOptions: {
        const minutes = [1, 2, 5, 10, 15, 30, 60, 120, 240]
        const current = root.screenLockSettings.timeoutMinutes
        if (minutes.indexOf(current) < 0) {
            minutes.push(current)
            minutes.sort(function(left, right) { return left - right })
        }
        return minutes.map(function(value) {
            return { "value": value, "label": qsTr("%1 minutes").arg(value) }
        })
    }

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
                    : qsTr("Automatic idle locking is off")
                onToggled: root.screenLockSettings.setAutomaticLock(checked)
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: root.screenLockSettings.automaticLock
                         && !root.screenLockSettings.busy
                spacing: Tokens.space["2"]

                Label {
                    text: qsTr("Lock after")
                    Accessible.name: text
                    muted: !parent.enabled
                }
                ComboBox {
                    id: timeoutSelector
                    objectName: "powerScreenLockTimeoutSelector"
                    Layout.fillWidth: true
                    enabled: parent.enabled
                    model: root.timeoutOptions
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.timeoutOptions.findIndex(function(option) {
                        return option.value === root.screenLockSettings.timeoutMinutes
                    })
                    accessibleDescription: qsTr("Choose the automatic idle-lock timeout")
                    // currentIndex follows the persisted model value. Only an
                    // interactive activation changes the setting, so a reload
                    // or live-configure completion cannot write it again.
                    onActivated: index => {
                        if (index >= 0 && index < root.timeoutOptions.length)
                            root.screenLockSettings.setTimeoutMinutes(
                                root.timeoutOptions[index].value)
                    }
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
