// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QindaQt.Controls 1.0 as C
import QindaQt.Tokens 1.0

Frame {
    id: root

    required property var row
    required property var access
    required property var colors
    property bool expanded: false

    objectName: "smartLightDeviceRow"

    // A short, high-contrast palette rather than a colour wheel: a panel popup
    // is for choosing a mood in one click, not for picking an exact tone.
    readonly property var swatches: [
        "#ff4d4d", "#ff8c1a", "#ffd633", "#7ed957",
        "#33d6c4", "#3399ff", "#8c5cff", "#ff66cc"
    ]

    function applySwatch(hex) {
        const color = Qt.color(hex)
        root.access.requestColor(root.row.deviceId,
                                 Math.round(color.r * 255),
                                 Math.round(color.g * 255),
                                 Math.round(color.b * 255))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    objectName: "smartLightDeviceLabel"
                    Layout.fillWidth: true
                    text: root.row.label
                    color: root.colors.text ?? "white"
                    elide: Text.ElideRight
                    Accessible.name: root.row.accessibleName
                    Accessible.description: root.row.accessibleDescription
                }

                Label {
                    objectName: "smartLightDeviceStatus"
                    Layout.fillWidth: true
                    text: root.row.statusLabel
                    color: root.row.reachable ? (root.colors.textMuted ?? "#a9afa9")
                                              : (root.colors.warning ?? "#e5a84b")
                    elide: Text.ElideRight
                }
            }

            C.Switch {
                objectName: "smartLightPowerSwitch"
                enabled: root.row.controllable
                checked: root.row.on
                accessibleDescription: qsTr("Switch %1 on or off").arg(root.row.label)
                onToggled: root.access.requestPower(root.row.deviceId, checked)
            }

            ToolButton {
                objectName: "smartLightExpandButton"
                text: root.expanded ? "▴" : "▾"
                focusPolicy: Qt.TabFocus
                Accessible.role: Accessible.Button
                Accessible.name: root.expanded
                    ? qsTr("Hide settings for %1").arg(root.row.label)
                    : qsTr("Show settings for %1").arg(root.row.label)
                onClicked: root.expanded = !root.expanded
                Accessible.onPressAction: root.expanded = !root.expanded
            }
        }

        SmartLightDeviceControls {
            Layout.fillWidth: true
            visible: root.expanded
            row: root.row
            access: root.access
            colors: root.colors
            swatches: root.swatches
            expanded: root.expanded
            onSwatchChosen: (hex) => root.applySwatch(hex)
        }
    }
}
