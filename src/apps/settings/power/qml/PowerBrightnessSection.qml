// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var powerSettings
    property Item firstActionTarget: null
    property var actionRegistrations: ({})
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    function updateAction(index, item) {
        root.actionRegistrations[index] = item
        root.refreshTarget()
    }
    function removeAction(index) {
        delete root.actionRegistrations[index]
        root.refreshTarget()
    }
    function refreshTarget() {
        const indices = Object.keys(root.actionRegistrations).map(Number)
        indices.sort((left, right) => left - right)
        root.firstActionTarget = null
        for (const index of indices) {
            const item = root.actionRegistrations[index]
            if (item !== null && item.enabled) {
                root.firstActionTarget = item
                break
            }
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Internal display brightness")
        description: qsTr("Exact observed values; Power1 version 1 has no display-brightness mutation")
    }

    Repeater {
        id: internalRepeater
        model: root.powerSettings.internalBrightnessRows

        delegate: FormSurface {
            id: internalRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: internalRow.modelData.name
            Accessible.description: internalRow.modelData.accessibleDescription

            contentItem: ColumnLayout {
                spacing: Tokens.space["2"]
                Label {
                    Layout.fillWidth: true
                    text: internalRow.modelData.name
                    font.weight: Font.DemiBold
                }
                Slider {
                    objectName: "powerInternalBrightness_" + internalRow.modelData.id
                    Layout.fillWidth: true
                    from: 0
                    to: 10000
                    stepSize: 100
                    value: internalRow.modelData.known
                           ? internalRow.modelData.normalized : 0
                    enabled: false
                    accessibleName: qsTr("%1 brightness").arg(internalRow.modelData.name)
                    accessibleDescription: internalRow.modelData.accessibleDescription
                }
                Label {
                    objectName: "powerInternalRaw_" + internalRow.modelData.id
                    Layout.fillWidth: true
                    text: qsTr("Normalized %1 of 10000 · %2 · %3")
                        .arg(internalRow.modelData.normalized)
                        .arg(internalRow.modelData.rawText)
                        .arg(internalRow.modelData.reason)
                    wrapMode: Text.Wrap
                    muted: true
                    Accessible.name: text
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: internalRepeater.count === 0
        text: qsTr("No internal display brightness device is currently reported.")
        muted: true
        Accessible.name: text
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Keyboard brightness")
        description: qsTr("Normalized 0–10000 controls with exact raw values")
    }

    Repeater {
        id: keyboardRepeater
        model: root.powerSettings.keyboardBrightnessRows

        delegate: FormSurface {
            id: keyboardRow
            required property var modelData
            required property int index
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: keyboardRow.modelData.name
            Accessible.description: keyboardRow.modelData.accessibleDescription

            contentItem: ColumnLayout {
                spacing: Tokens.space["2"]
                Label {
                    Layout.fillWidth: true
                    text: keyboardRow.modelData.name
                    font.weight: Font.DemiBold
                }
                Slider {
                    id: keyboardSlider
                    objectName: "powerKeyboardBrightness_" + keyboardRow.modelData.id
                    Layout.fillWidth: true
                    from: 0
                    to: 10000
                    stepSize: 100
                    value: keyboardRow.modelData.known
                           ? keyboardRow.modelData.normalized : 0
                    enabled: keyboardRow.modelData.available
                    accessibleName: qsTr("%1 brightness").arg(keyboardRow.modelData.name)
                    accessibleDescription: qsTr("%1. Current normalized value %2 of 10000. %3")
                        .arg(keyboardRow.modelData.accessibleDescription)
                        .arg(Math.round(value))
                        .arg(keyboardRow.modelData.rawText)
                    onEnabledChanged: root.updateAction(keyboardRow.index,
                                                        keyboardSlider)
                    Component.onCompleted: root.updateAction(keyboardRow.index,
                                                              keyboardSlider)
                    Component.onDestruction: root.removeAction(keyboardRow.index)
                    onMoved: if (enabled) root.powerSettings.requestKeyboardBrightness(
                                 keyboardRow.modelData.id, Math.round(value))
                }
                Label {
                    objectName: "powerKeyboardRaw_" + keyboardRow.modelData.id
                    Layout.fillWidth: true
                    text: qsTr("Normalized %1 of 10000 · %2")
                        .arg(keyboardRow.modelData.normalized)
                        .arg(keyboardRow.modelData.rawText)
                    muted: true
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: keyboardRepeater.count === 0
        text: qsTr("No keyboard backlight is currently reported.")
        muted: true
        Accessible.name: text
    }
}
