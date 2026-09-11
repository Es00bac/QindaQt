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

    // AGENT-CONTRACT: lidPolicy is the injected PowerButtonLidPolicyPort
    // surface (ADR-0132); powerSettings supplies the validated Power1 lid
    // presence fact. Both lid rows hide without a lid; the power-button row
    // stays, because every machine has a power button.
    required property var powerSettings
    required property var lidPolicy
    readonly property bool lidPresent: powerSettings.lidPresent
    // ADR-0132: the offered actions mirror the supported
    // PowerDevil::PowerButtonAction values; the numbers are wire contract.
    // buildOptions() stays pure so repeated binding evaluations never
    // accumulate duplicate out-of-set entries.
    function buildOptions(action) {
        const options = [
            { "value": 0, "label": qsTr("Do nothing") },
            { "value": 1, "label": qsTr("Sleep") },
            { "value": 2, "label": qsTr("Hibernate") },
            { "value": 8, "label": qsTr("Shut down") },
            { "value": 32, "label": qsTr("Lock screen") },
            { "value": 64, "label": qsTr("Turn off screen") }
        ]
        if (options.findIndex(function(option) {
                return option.value === action }) < 0)
            options.push({ "value": action, "label": String(action) })
        return options
    }
    readonly property Item firstActionTarget:
        powerButtonSelector.enabled ? powerButtonSelector : null

    Layout.fillWidth: true
    spacing: Tokens.space["2"]
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Power button and lid")

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Power button and lid")
        description: qsTr("Choose what PowerDevil does for buttons and the lid")
    }

    FormSurface {
        Layout.fillWidth: true
        padding: Tokens.space["3"]

        contentItem: ColumnLayout {
            spacing: Tokens.space["2"]

            RowLayout {
                Layout.fillWidth: true
                enabled: root.lidPolicy.available && !root.lidPolicy.busy
                spacing: Tokens.space["2"]

                ShellIcons.Icon {
                    objectName: "powerButtonActionIcon"
                    name: "system-shutdown"
                    size: 18
                    fallbackText: qsTr("Power button")
                    Accessible.ignored: true
                }
                Label {
                    text: qsTr("When the power button is pressed")
                    Accessible.name: text
                    muted: !parent.enabled
                }
                ComboBox {
                    id: powerButtonSelector
                    objectName: "powerPowerButtonSelector"
                    Layout.fillWidth: true
                    enabled: parent.enabled
                    readonly property var options:
                        root.buildOptions(root.lidPolicy.powerButtonAction)
                    model: options
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: options.findIndex(function(option) {
                        return option.value === root.lidPolicy.powerButtonAction
                    })
                    T.ToolTip.visible: powerButtonHover.hovered
                    T.ToolTip.delay: 600
                    T.ToolTip.text: qsTr(
                        "PowerDevil runs this action when the power button is pressed")
                    accessibleDescription: qsTr(
                        "Choose the power button action")
                    onActivated: index => {
                        if (index >= 0 && index < options.length)
                            root.lidPolicy.applyPolicy(
                                root.lidPolicy.lidAction,
                                root.lidPolicy.wakeWithExternalMonitor,
                                options[index].value)
                    }
                    HoverHandler { id: powerButtonHover }
                }
            }

            RowLayout {
                id: lidRow
                objectName: "powerLidActionRow"
                Layout.fillWidth: true
                visible: root.lidPresent
                enabled: root.lidPolicy.available && !root.lidPolicy.busy
                spacing: Tokens.space["2"]

                ShellIcons.Icon {
                    objectName: "powerLidActionIcon"
                    name: "computer-laptop"
                    size: 18
                    fallbackText: qsTr("Lid")
                    Accessible.ignored: true
                }
                Label {
                    text: qsTr("When the lid is closed")
                    Accessible.name: text
                    muted: !parent.enabled
                }
                ComboBox {
                    id: lidSelector
                    objectName: "powerLidActionSelector"
                    Layout.fillWidth: true
                    enabled: parent.enabled
                    readonly property var options:
                        root.buildOptions(root.lidPolicy.lidAction)
                    model: options
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: options.findIndex(function(option) {
                        return option.value === root.lidPolicy.lidAction
                    })
                    T.ToolTip.visible: lidHover.hovered
                    T.ToolTip.delay: 600
                    T.ToolTip.text: qsTr(
                        "PowerDevil runs this action when the lid closes")
                    accessibleDescription: qsTr("Choose the lid close action")
                    onActivated: index => {
                        if (index >= 0 && index < options.length)
                            root.lidPolicy.applyPolicy(
                                options[index].value,
                                root.lidPolicy.wakeWithExternalMonitor,
                                root.lidPolicy.powerButtonAction)
                    }
                    HoverHandler { id: lidHover }
                }
            }

            RowLayout {
                id: externalMonitorRow
                objectName: "powerLidExternalMonitorRow"
                Layout.fillWidth: true
                visible: root.lidPresent
                spacing: Tokens.space["2"]

                ShellIcons.Icon {
                    objectName: "powerLidExternalMonitorIcon"
                    name: "video-display"
                    size: 18
                    fallbackText: qsTr("External monitor")
                    Accessible.ignored: true
                }
                Switch {
                    id: externalMonitor
                    objectName: "powerLidExternalMonitorSwitch"
                    Layout.fillWidth: true
                    text: qsTr("Run the lid action even with an external monitor")
                    checked: root.lidPolicy.wakeWithExternalMonitor
                    enabled: root.lidPolicy.available && !root.lidPolicy.busy
                    accessibleDescription: checked
                        ? qsTr("Closing the lid runs its action even when an external monitor is connected")
                        : qsTr("Closing the lid is ignored while an external monitor is connected")
                    onToggled: root.lidPolicy.applyPolicy(
                        root.lidPolicy.lidAction, checked,
                        root.lidPolicy.powerButtonAction)
                }
            }

            Label {
                objectName: "powerLidPolicyError"
                Layout.fillWidth: true
                visible: root.lidPolicy.errorText.length > 0
                text: root.lidPolicy.errorText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }

        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Button and lid actions")
    }
}
