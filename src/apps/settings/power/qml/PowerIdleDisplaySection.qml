// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

ColumnLayout {
    id: root

    required property var idleDisplaySettings
    property Item firstActionTarget: null
    property var actionRegistrations: ({})

    Layout.fillWidth: true
    spacing: Tokens.space["2"]
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Display power")

    function sourceIds(rows) {
        return rows.map(row => row.source)
    }
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
        title: qsTr("Display power")
        description: qsTr("Choose the idle display-off timeout for each power source")
    }

    Repeater {
        id: sourceCards
        model: root.sourceIds(root.idleDisplaySettings.sourceRows)

        delegate: FormSurface {
            id: sourceCard
            required property int index
            readonly property var row: root.idleDisplaySettings.sourceRows[index] ?? ({})
            Layout.fillWidth: true
            Component.onCompleted: root.updateAction(index, displayOff)
            Component.onDestruction: root.removeAction(index)
            padding: Tokens.space["3"]
            Accessible.role: Accessible.Grouping
            Accessible.name: sourceCard.row.label
            Accessible.description: sourceCard.row.active
                ? qsTr("This is the active power source")
                : sourceCard.row.sourceLayer

            contentItem: ColumnLayout {
                spacing: Tokens.space["2"]

                Label {
                    Layout.fillWidth: true
                    text: sourceCard.row.active
                        ? qsTr("%1 · active").arg(sourceCard.row.label)
                        : sourceCard.row.label
                    font.weight: Font.DemiBold
                }

                Switch {
                    id: displayOff
                    objectName: "powerIdleDisplayOff_" + sourceCard.row.source
                    Layout.fillWidth: true
                    text: qsTr("Turn the display off when idle")
                    checked: sourceCard.row.enabled
                    enabled: sourceCard.row.confirmed
                             && root.idleDisplaySettings.available
                             && !root.idleDisplaySettings.busy
                    onEnabledChanged: root.refreshTarget()
                    accessibleDescription: checked
                        ? qsTr("The display-off timeout is enabled for %1")
                            .arg(sourceCard.row.label)
                        : qsTr("The display stays on during inactivity for %1")
                            .arg(sourceCard.row.label)
                    onClicked: {
                        const sourceKey = sourceCard.row.source
                        const settingsModel = root.idleDisplaySettings
                        const rowIndex = sourceCard.index
                        settingsModel.setEnabled(
                            sourceKey, !sourceCard.row.enabled)
                        Qt.callLater(function() {
                            if (!displayOff) return
                            displayOff.checked = Qt.binding(function() {
                                const rows = settingsModel.sourceRows
                                if (!rows || rows.length <= rowIndex || !rows[rowIndex])
                                    return false
                                return rows[rowIndex].enabled
                            })
                        })
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    enabled: sourceCard.row.confirmed
                             && sourceCard.row.enabled
                             && root.idleDisplaySettings.available
                             && !root.idleDisplaySettings.busy
                    spacing: Tokens.space["2"]

                    Label {
                        text: qsTr("Turn off after")
                        Accessible.name: text
                        muted: !parent.enabled
                    }
                    T.SpinBox {
                        id: timeout
                        objectName: "powerIdleDisplayOffSeconds_" + sourceCard.row.source
                        Layout.fillWidth: true
                        from: 0
                        to: 14400
                        editable: true
                        value: sourceCard.row.seconds
                        Accessible.description: qsTr("Display-off idle timeout in seconds; zero means never")
                        onValueModified: root.idleDisplaySettings.setSeconds(
                            sourceCard.row.source, value)
                    }
                    Label {
                        text: timeout.value === 0 ? qsTr("Never") : qsTr("seconds")
                        muted: !parent.enabled
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Effective value source: %1").arg(sourceCard.row.sourceLayer)
                    wrapMode: Text.Wrap
                    muted: true
                }
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
        visible: !root.idleDisplaySettings.available
                 || root.idleDisplaySettings.errorText.length > 0
        text: qsTr("Refresh display preferences")
        available: !root.idleDisplaySettings.busy
        accessibleDescription: qsTr("Read current per-source display-off preferences without repeating a change")
        onClicked: root.idleDisplaySettings.retry()
    }
}
