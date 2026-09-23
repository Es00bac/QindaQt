// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var idleDisplaySettings
    readonly property Item firstActionTarget: displayOff.enabled ? displayOff
                                             : retryAction.visible ? retryAction : null
    readonly property var timeoutOptions: {
        const minutes = [1, 2, 5, 10, 15, 30, 60, 120, 240]
        const current = root.idleDisplaySettings.minutes
        if (current >= 1 && minutes.indexOf(current) < 0) {
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
                visible: root.idleDisplaySettings.hasConfirmed
                checked: root.idleDisplaySettings.enabled
                enabled: root.idleDisplaySettings.canEdit
                accessibleDescription: checked
                    ? qsTr("The display turns off after the selected idle time")
                    : qsTr("The display stays on during inactivity")
                // A click is intent, not persisted policy. Restore the
                // binding after Qt toggles its internal checked property.
                onClicked: {
                    root.idleDisplaySettings.setEnabled(
                        !root.idleDisplaySettings.enabled)
                    Qt.callLater(() => displayOff.checked = Qt.binding(
                        () => root.idleDisplaySettings.enabled))
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.idleDisplaySettings.hasConfirmed
                enabled: root.idleDisplaySettings.enabled
                         && root.idleDisplaySettings.canEdit
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
                        Qt.callLater(() => timeoutSelector.currentIndex = Qt.binding(
                            () => root.timeoutOptions.findIndex(option =>
                                option.value === root.idleDisplaySettings.minutes)))
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
                id: retryAction
                objectName: "powerIdleDisplayRetry"
                visible: !root.idleDisplaySettings.available
                         || root.idleDisplaySettings.errorText.length > 0
                text: qsTr("Refresh display preference")
                available: !root.idleDisplaySettings.busy
                accessibleDescription: qsTr("Read the current display-off preference without repeating a change")
                onClicked: root.idleDisplaySettings.retry()
            }
        }
    }
}
