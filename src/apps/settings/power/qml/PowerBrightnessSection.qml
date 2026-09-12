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

    // AGENT-GUARD: repeaters iterate row IDs, not row maps. The model
    // republishes every row on each request and fence change; an unchanged ID
    // list keeps each delegate, so a focused slider keeps keyboard focus and a
    // pointer drag survives republication. Delegates read their row by index.
    function rowIds(rows) {
        return rows.map(row => row.id)
    }
    // Internal panels register first (keys 0-99), keyboard backlights after.
    function updateAction(key, item) {
        root.actionRegistrations[key] = item
        root.refreshTarget()
    }
    function removeAction(key) {
        delete root.actionRegistrations[key]
        root.refreshTarget()
    }
    function refreshTarget() {
        const keys = Object.keys(root.actionRegistrations).map(Number)
        keys.sort((left, right) => left - right)
        root.firstActionTarget = null
        for (const key of keys) {
            const item = root.actionRegistrations[key]
            if (item !== null && item.enabled) {
                root.firstActionTarget = item
                break
            }
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Internal display brightness")
        description: qsTr("Adjust the built-in display backlight when this computer allows it.")
    }

    Repeater {
        id: internalRepeater
        model: root.rowIds(root.powerSettings.internalBrightnessRows)

        delegate: FormSurface {
            id: internalRow
            required property int index
            readonly property var row: root.powerSettings.internalBrightnessRows[index] ?? ({})
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: internalRow.row.name ?? ""
            Accessible.description: internalRow.row.accessibleDescription ?? ""

            contentItem: ColumnLayout {
                spacing: Tokens.space["2"]
                Label {
                    Layout.fillWidth: true
                    text: internalRow.row.name ?? ""
                    font.weight: Font.DemiBold
                }
                Slider {
                    id: internalSlider
                    objectName: "powerInternalBrightnessSlider_" + (internalRow.row.id ?? "")
                    Layout.fillWidth: true
                    visible: internalRow.row.known === true
                    from: 0
                    to: 10000
                    stepSize: 100
                    value: internalRow.row.known === true ? internalRow.row.normalized : 0
                    enabled: internalRow.row.available === true
                    accessibleName: qsTr("%1 brightness").arg(internalRow.row.name ?? "")
                    accessibleDescription: internalRow.row.accessibleDescription ?? ""
                    onEnabledChanged: root.updateAction(internalRow.index, internalSlider)
                    Component.onCompleted: root.updateAction(internalRow.index, internalSlider)
                    Component.onDestruction: root.removeAction(internalRow.index)
                    onMoved: if (enabled) root.powerSettings.requestInternalBrightness(
                                 internalRow.row.id, Math.round(value))
                }
                Label {
                    objectName: "powerInternalBrightness_" + (internalRow.row.id ?? "")
                    Layout.fillWidth: true
                    text: internalRow.row.known === true
                        ? qsTr("Brightness: %1%").arg(Math.round(internalRow.row.normalized / 100))
                        : qsTr("Brightness is unavailable")
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
                Label {
                    objectName: "powerInternalRaw_" + (internalRow.row.id ?? "")
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: internalRow.row.reason ?? ""
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
        model: root.rowIds(root.powerSettings.keyboardBrightnessRows)

        delegate: FormSurface {
            id: keyboardRow
            required property int index
            readonly property var row: root.powerSettings.keyboardBrightnessRows[index] ?? ({})
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: keyboardRow.row.name ?? ""
            Accessible.description: keyboardRow.row.accessibleDescription ?? ""

            contentItem: ColumnLayout {
                spacing: Tokens.space["2"]
                Label {
                    Layout.fillWidth: true
                    text: keyboardRow.row.name ?? ""
                    font.weight: Font.DemiBold
                }
                Slider {
                    id: keyboardSlider
                    objectName: "powerKeyboardBrightness_" + (keyboardRow.row.id ?? "")
                    Layout.fillWidth: true
                    from: 0
                    to: 10000
                    stepSize: 100
                    value: keyboardRow.row.known === true ? keyboardRow.row.normalized : 0
                    enabled: keyboardRow.row.available === true
                    accessibleName: qsTr("%1 brightness").arg(keyboardRow.row.name ?? "")
                    accessibleDescription: qsTr("%1. Brightness %2%")
                        .arg(keyboardRow.row.name ?? "")
                        .arg(Math.round(value / 100))
                    onEnabledChanged: root.updateAction(100 + keyboardRow.index,
                                                        keyboardSlider)
                    Component.onCompleted: root.updateAction(100 + keyboardRow.index,
                                                             keyboardSlider)
                    Component.onDestruction: root.removeAction(100 + keyboardRow.index)
                    onMoved: if (enabled) root.powerSettings.requestKeyboardBrightness(
                                 keyboardRow.row.id, Math.round(value))
                }
                Label {
                    objectName: "powerKeyboardRaw_" + (keyboardRow.row.id ?? "")
                    Layout.fillWidth: true
                    text: qsTr("Brightness: %1%")
                        .arg(Math.round((keyboardRow.row.normalized ?? 0) / 100))
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
