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

    required property var networkSettings
    // Width of every row's trailing status/action area ("Saved", or the
    // prompt text and Connect). Derived only from the section width, so it is
    // identical in every row; see the AGENT-GUARD on the row layout.
    readonly property real trailingWidth: Math.round(
        Math.min(200, Math.max(120, root.width / 3)))
    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    // AGENT-NOTE: Tk.Theme is an engine singleton that keeps QindaTK's own
    // preset until a bridge feeds it the desktop's tokens. The signal meter
    // must wear the session theme even when no other QindaTK route has been
    // opened, so this section carries its own bridge (bridges are idempotent).
    QindaQtTheme {}

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Wi-Fi networks")
        description: qsTr("Choose a network to connect. Secured networks ask for a password.")
    }

    Repeater {
        id: accessPointRepeater
        model: root.networkSettings.accessPoints

        delegate: FormSurface {
            id: accessPointRow
            required property var modelData
            Layout.fillWidth: true
            padding: Tokens.space["3"]
            Accessible.name: qsTr("%1, %2, signal %3 percent on %4")
                .arg(accessPointRow.modelData.displayName)
                .arg(accessPointRow.modelData.securityText)
                .arg(accessPointRow.modelData.signalStrength)
                .arg(accessPointRow.modelData.deviceInterface)
            Accessible.description: accessPointRow.modelData.promptStatusText

            // AGENT-GUARD: every row's signal meter must start at the same x.
            // The meter, the percentage text and the trailing area therefore
            // have fixed widths (minimum = preferred = maximum) that are equal
            // in every row, and only the SSID column absorbs a row's
            // differences. Sizing any of them from the row's own content
            // (the digits, "Saved" versus Connect, the prompt length) makes the
            // bars ragged again; tst_network_page asserts the alignment.
            contentItem: RowLayout {
                spacing: Tokens.space["3"]

                // Same text and font in every row, so the same width.
                TextMetrics {
                    id: percentMetrics
                    font: percentLabel.font
                    text: qsTr("%1%").arg(100)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.space["1"]

                    Label {
                        Layout.fillWidth: true
                        text: accessPointRow.modelData.displayName
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 · %2 · %3 MHz")
                            .arg(accessPointRow.modelData.securityText)
                            .arg(accessPointRow.modelData.deviceInterface)
                            .arg(accessPointRow.modelData.frequencyMHz)
                        muted: true
                    }
                }

                // AGENT-CONTRACT: the meter repeats the percentage beside it;
                // it never replaces the text, so nothing is carried by colour
                // or bar length alone. High signal is good, so the fill is the
                // neutral accent, not QindaTK's load ramp (high = bad).
                Tk.Meter {
                    id: signalMeter
                    objectName: "networkSignalMeter_" + accessPointRow.modelData.id
                    Layout.minimumWidth: 64
                    Layout.preferredWidth: 64
                    Layout.maximumWidth: 64
                    Layout.preferredHeight: implicitHeight
                    Layout.alignment: Qt.AlignVCenter
                    from: 0
                    to: 100
                    value: accessPointRow.modelData.signalStrength
                    color: Tk.Theme.color.accent
                    trackColor: Tk.Theme.color.divider
                    tooltip: qsTr("Signal strength %1 percent")
                        .arg(accessPointRow.modelData.signalStrength)
                    Accessible.role: Accessible.ProgressBar
                    Accessible.name: qsTr("Signal strength bar, %1 percent")
                        .arg(accessPointRow.modelData.signalStrength)

                    Tk.ToolTip {
                        text: signalMeter.tooltip
                        visible: signalMeter.hovered
                    }
                }

                Label {
                    id: percentLabel
                    readonly property real fixedWidth: Math.ceil(percentMetrics.advanceWidth)
                    Layout.minimumWidth: percentLabel.fixedWidth
                    Layout.preferredWidth: percentLabel.fixedWidth
                    Layout.maximumWidth: percentLabel.fixedWidth
                    horizontalAlignment: Text.AlignRight
                    wrapMode: Text.NoWrap
                    text: qsTr("%1%").arg(accessPointRow.modelData.signalStrength)
                    Accessible.name: qsTr("Signal strength %1 percent")
                        .arg(accessPointRow.modelData.signalStrength)
                }

                // Fixed width, equal in every row; see its AGENT-GUARD.
                NetworkAccessPointActions {
                    accessPoint: accessPointRow.modelData
                    networkSettings: root.networkSettings
                    baseWidth: root.trailingWidth
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: accessPointRepeater.count === 0
        text: qsTr("No access points are currently reported.")
        muted: true
    }
}
