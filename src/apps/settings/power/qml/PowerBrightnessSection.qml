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
        description: qsTr("Current display brightness. Use your display’s controls to adjust it.")
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
                Label {
                    objectName: "powerInternalBrightness_" + internalRow.modelData.id
                    Layout.fillWidth: true
                    text: internalRow.modelData.known
                        ? qsTr("Brightness: %1%").arg(Math.round(internalRow.modelData.normalized / 100))
                        : qsTr("Brightness is unavailable")
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
                Label {
                    objectName: "powerInternalRaw_" + internalRow.modelData.id
                    Layout.fillWidth: true
                    text: internalRow.modelData.reason
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
        description: qsTr("Adjust the keyboard backlight.")
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
                    accessibleDescription: qsTr("%1. Brightness %2%")
                        .arg(keyboardRow.modelData.name)
                        .arg(Math.round(value / 100))
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
                    text: qsTr("Brightness: %1%")
                        .arg(Math.round(keyboardRow.modelData.normalized / 100))
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
