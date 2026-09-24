// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaTK as Tk
import QindaTK.QindaQt

ColumnLayout {
    id: root

    required property var powerSettings
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    // AGENT-NOTE: Tk.Theme is an engine singleton that keeps QindaTK's own
    // preset until a bridge feeds it the desktop's tokens. The charge meter
    // must wear the session theme even when no other QindaTK route has been
    // opened, so this section carries its own bridge (bridges are idempotent).
    QindaQtTheme {}

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Power supplies")
        description: qsTr("Batteries, UPS devices, adapters, estimates, and warning severity")
    }

    Repeater {
        id: supplyRepeater
        model: root.powerSettings.supplyRows

        delegate: FormSurface {
            id: supplyRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.role: Accessible.ListItem
            Accessible.name: supplyRow.modelData.name
            Accessible.description: supplyRow.modelData.accessibleDescription

            contentItem: GridLayout {
                objectName: "powerSupplyLayout_" + supplyRow.modelData.id
                columns: supplyRow.width < 500 ? 1 : 2
                columnSpacing: Tokens.space["4"]
                rowSpacing: Tokens.space["1"]

                // AGENT-GUARD: both columns take equal shares of the row
                // (equal preferred widths, both filling). Every row is a
                // separate grid, so a column sized from its own text would
                // start the charge meters at a different x in each row.
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: Tokens.space["1"]
                    Label {
                        Layout.fillWidth: true
                        text: supplyRow.modelData.name
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: supplyRow.modelData.kindText
                        muted: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: Tokens.space["1"]
                    Label {
                        objectName: "powerSupplyState_" + supplyRow.modelData.id
                        Layout.fillWidth: true
                        text: [supplyRow.modelData.stateText,
                               supplyRow.modelData.percentageText,
                               supplyRow.modelData.timeText]
                              .filter(value => value.length > 0).join(" · ")
                    }
                    // AGENT-CONTRACT: an unknown charge draws no meter at all,
                    // never an empty bar (unknown is not zero). High charge is
                    // good, so the fill is the neutral accent rather than a
                    // load ramp; it changes colour only when Power itself
                    // raises a warning (Low = 3, Critical/Action >= 4, the same
                    // thresholds as the warning label below, which always
                    // states the warning in words). The warning hues come
                    // from the desktop tokens, as the panel's audio meter's
                    // do: Tk.Theme.color.warning is bridged from the status
                    // pair's text-on-warning half, which is near-black on dark
                    // themes and near-white on light ones.
                    Tk.Meter {
                        id: chargeMeter
                        objectName: "powerSupplyMeter_" + supplyRow.modelData.id
                        visible: supplyRow.modelData.percentageKnown === true
                        Layout.fillWidth: true
                        Layout.maximumWidth: 240
                        Layout.preferredHeight: implicitHeight
                        from: 0
                        to: 100
                        value: supplyRow.modelData.percentageKnown === true
                               ? supplyRow.modelData.percentage : 0
                        color: supplyRow.modelData.warningSeverity >= 4
                               ? Tokens.danger.default
                               : supplyRow.modelData.warningSeverity >= 3
                                 ? Tokens.status.warning.background
                                 : Tk.Theme.color.accent
                        trackColor: Tk.Theme.color.divider
                        tooltip: qsTr("%1 charge %2 percent")
                            .arg(supplyRow.modelData.name)
                            .arg(Math.round(chargeMeter.value))
                        Accessible.role: Accessible.ProgressBar
                        Accessible.name: qsTr("%1 charge bar, %2 percent")
                            .arg(supplyRow.modelData.name)
                            .arg(Math.round(chargeMeter.value))

                        Tk.ToolTip {
                            text: chargeMeter.tooltip
                            visible: chargeMeter.hovered
                        }
                    }
                    Label {
                        objectName: "powerSupplyWarning_" + supplyRow.modelData.id
                        Layout.fillWidth: true
                        text: supplyRow.modelData.warningText
                        color: supplyRow.modelData.warningSeverity >= 4
                               ? Tokens.fg.default
                               : supplyRow.modelData.warningSeverity >= 3
                                 ? Tokens.fg.default
                                 : Tokens.fg.muted
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: supplyRepeater.count === 0
        text: qsTr("No power supplies are currently reported.")
        muted: true
        Accessible.name: text
    }
}
