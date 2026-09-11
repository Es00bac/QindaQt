// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.Shell.Icons 1.0 as ShellIcons

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
    // ADR-0132: the grace ladder mirrors the documented KScreenLocker choices
    // in seconds. A stored out-of-set value (an upstream custom delay) is kept
    // as an extra entry so opening the page never silently changes it.
    readonly property var graceOptions: {
        const seconds = [
            { "value": 0, "label": qsTr("Immediately") },
            { "value": 5, "label": qsTr("5 seconds") },
            { "value": 30, "label": qsTr("30 seconds") },
            { "value": 60, "label": qsTr("1 minute") },
            { "value": 300, "label": qsTr("5 minutes") }
        ]
        const current = root.screenLockSettings.lockGraceSeconds
        if (seconds.findIndex(function(option) { return option.value === current }) < 0)
            seconds.push({ "value": current,
                           "label": qsTr("%1 seconds").arg(current) })
        seconds.sort(function(left, right) { return left.value - right.value })
        return seconds
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

                ShellIcons.Icon {
                    objectName: "powerScreenLockTimeoutIcon"
                    name: "chronometer"
                    size: 18
                    fallbackText: qsTr("Idle lock timer")
                    Accessible.ignored: true
                }
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

            Switch {
                id: lockOnResume
                objectName: "powerScreenLockOnResume"
                Layout.fillWidth: true
                text: qsTr("Lock after waking from sleep")
                checked: root.screenLockSettings.lockOnResume
                enabled: !root.screenLockSettings.busy
                accessibleDescription: checked
                    ? qsTr("The screen locks when the computer wakes from sleep")
                    : qsTr("Waking from sleep does not lock the screen")
                onToggled: root.screenLockSettings.setLockOnResume(checked)
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: !root.screenLockSettings.busy
                spacing: Tokens.space["2"]

                ShellIcons.Icon {
                    objectName: "powerScreenLockGraceIcon"
                    name: "user-away"
                    size: 18
                    fallbackText: qsTr("Unlock delay")
                    Accessible.ignored: true
                }
                Label {
                    text: qsTr("Require password after")
                    Accessible.name: text
                    muted: !parent.enabled
                }
                ComboBox {
                    id: graceSelector
                    objectName: "powerScreenLockGraceSelector"
                    Layout.fillWidth: true
                    enabled: parent.enabled
                    model: root.graceOptions
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.graceOptions.findIndex(function(option) {
                        return option.value === root.screenLockSettings.lockGraceSeconds
                    })
                    T.ToolTip.visible: graceHover.hovered
                    T.ToolTip.delay: 600
                    T.ToolTip.text: qsTr("How long the screen stays unlocked after it locks")
                    accessibleDescription: qsTr("Choose the unlock grace period")
                    onActivated: index => {
                        if (index >= 0 && index < root.graceOptions.length)
                            root.screenLockSettings.setLockGraceSeconds(
                                root.graceOptions[index].value)
                    }
                    HoverHandler { id: graceHover }
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

        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Automatic screen lock")
    }
}
