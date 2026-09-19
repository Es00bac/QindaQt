// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var screensaverSettings
    readonly property Item firstActionTarget: saverSelector.enabled ? saverSelector : null

    // AGENT-GUARD: this list, ScreensaverPreferences::knownSavers(), and the
    // schema's allowedValues for power.screensaver are one set in three
    // places. A token offered here that the others do not know is refused
    // before it is ever written.
    readonly property var saverOptions: [
        { "value": "none", "label": qsTr("None") },
        { "value": "qinda-patrol", "label": qsTr("Qinda Patrol") },
        { "value": "circuit-reef", "label": qsTr("Circuit Reef") },
        { "value": "prism-circuit", "label": qsTr("Prism Circuit") },
        { "value": "prism-brawl", "label": qsTr("Prism Brawl") },
        { "value": "starward", "label": qsTr("Starward") }
    ]
    readonly property var delayOptions: {
        const minutes = [1, 2, 5, 10, 15, 30, 60]
        const current = root.screensaverSettings.minutes
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
    Accessible.name: qsTr("Screensaver")

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Screensaver")
        description: qsTr("Choose what the screen shows when the session goes idle")
    }

    FormSurface {
        Layout.fillWidth: true
        padding: Tokens.space["3"]
        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Idle screensaver")

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            RowLayout {
                Layout.fillWidth: true
                enabled: !root.screensaverSettings.busy
                spacing: Tokens.space["2"]

                Label {
                    text: qsTr("Show")
                    Accessible.name: text
                    muted: !parent.enabled
                }
                ComboBox {
                    id: saverSelector
                    objectName: "powerScreensaverSelector"
                    Layout.fillWidth: true
                    enabled: parent.enabled
                    model: root.saverOptions
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.saverOptions.findIndex(function(option) {
                        return option.value === root.screensaverSettings.saver
                    })
                    accessibleDescription: qsTr("Choose the idle screensaver")
                    // Only an interactive activation writes; a confirmed
                    // snapshot refresh can never replay the selection.
                    onActivated: index => {
                        if (index >= 0 && index < root.saverOptions.length)
                            root.screensaverSettings.setSaver(
                                root.saverOptions[index].value)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: root.screensaverSettings.enabled
                         && !root.screensaverSettings.busy
                spacing: Tokens.space["2"]

                Label {
                    text: qsTr("Start after")
                    Accessible.name: text
                    muted: !parent.enabled
                }
                ComboBox {
                    id: delaySelector
                    objectName: "powerScreensaverDelaySelector"
                    Layout.fillWidth: true
                    enabled: parent.enabled
                    model: root.delayOptions
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.delayOptions.findIndex(function(option) {
                        return option.value === root.screensaverSettings.minutes
                    })
                    accessibleDescription: qsTr("Choose how long the session waits before the screensaver starts")
                    onActivated: index => {
                        if (index >= 0 && index < root.delayOptions.length)
                            root.screensaverSettings.setMinutes(
                                root.delayOptions[index].value)
                    }
                }
            }

            Label {
                objectName: "powerScreensaverStatus"
                Layout.fillWidth: true
                text: root.screensaverSettings.statusText
                wrapMode: Text.Wrap
                muted: true
                Accessible.name: text
            }
            Label {
                objectName: "powerScreensaverNote"
                Layout.fillWidth: true
                text: qsTr("A screensaver does not lock the session. Any activity dismisses it, and it stops when the screen locks. Automatic locking stays under Screen lock.")
                wrapMode: Text.Wrap
                muted: true
                Accessible.name: text
            }
            Label {
                objectName: "powerScreensaverError"
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.screensaverSettings.errorText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
            Button {
                objectName: "powerScreensaverRetry"
                visible: root.screensaverSettings.errorText.length > 0
                text: qsTr("Try again")
                available: !root.screensaverSettings.busy
                accessibleDescription: qsTr("Retry the failed screensaver preference step")
                onClicked: root.screensaverSettings.retry()
            }
        }
    }
}
