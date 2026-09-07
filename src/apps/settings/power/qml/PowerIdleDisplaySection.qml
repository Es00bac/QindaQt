// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var idleDisplaySettings
    readonly property Item firstActionTarget: displayOff.enabled ? displayOff : null
    readonly property var timeoutOptions: {
        const minutes = [1, 2, 5, 10, 15, 30, 60, 120, 240]
        const current = root.idleDisplaySettings.minutes
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
    Accessible.name: qsTr("Display power")

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Display power")
        description: qsTr("Choose whether inactivity turns the display off")
    }

    FormSurface {
        Layout.fillWidth: true
        padding: Tokens.space["3"]
        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Idle display-off")

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            Switch {
                id: displayOff
                objectName: "powerIdleDisplayOff"
                Layout.fillWidth: true
                text: qsTr("Turn the display off when idle")
                checked: root.idleDisplaySettings.enabled
                enabled: !root.idleDisplaySettings.busy
                accessibleDescription: checked
                    ? qsTr("The display turns off after the selected idle time")
                    : qsTr("The display stays on during inactivity")
                onToggled: root.idleDisplaySettings.setEnabled(checked)
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: root.idleDisplaySettings.enabled
                         && !root.idleDisplaySettings.busy
                spacing: Tokens.space["2"]

                Label {
                    text: qsTr("Turn off after")
                    Accessible.name: text
                    muted: !parent.enabled
                }
                ComboBox {
                    id: timeoutSelector
                    objectName: "powerIdleDisplayOffTimeoutSelector"
                    Layout.fillWidth: true
                    enabled: parent.enabled
                    model: root.timeoutOptions
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.timeoutOptions.findIndex(function(option) {
                        return option.value === root.idleDisplaySettings.minutes
                    })
                    accessibleDescription: qsTr("Choose the idle display-off timeout")
                    // Only an interactive activation writes; a confirmed
                    // snapshot refresh can never replay the selection.
                    onActivated: index => {
                        if (index >= 0 && index < root.timeoutOptions.length)
                            root.idleDisplaySettings.setMinutes(
                                root.timeoutOptions[index].value)
                    }
                }
            }

            Label {
                objectName: "powerIdleDisplayStatus"
                Layout.fillWidth: true
                text: root.idleDisplaySettings.statusText
                wrapMode: Text.Wrap
                muted: true
                Accessible.name: text
            }
            Label {
                objectName: "powerIdleDisplayError"
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.idleDisplaySettings.errorText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
            Button {
                objectName: "powerIdleDisplayRetry"
                visible: root.idleDisplaySettings.errorText.length > 0
                text: qsTr("Try again")
                available: !root.idleDisplaySettings.busy
                accessibleDescription: qsTr("Retry the failed display-off preference step")
                onClicked: root.idleDisplaySettings.retry()
            }
        }
    }
}
