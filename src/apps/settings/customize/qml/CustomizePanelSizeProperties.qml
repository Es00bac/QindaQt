// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Panel size editor: two sliders with a live value chip. The chip answers
// "what number am I changing" without a text-heavy form row.
FormSurface {
    id: root

    required property var customizeSettings
    required property var properties

    component ValueChip: Rectangle {
        id: chip

        property string valueText: ""
        property string chipToolTip: ""

        radius: Tokens.radius.s
        color: Tokens.bg.highest
        border.width: Tokens.space["1"] / 2
        border.color: Tokens.outline.divider
        implicitWidth: valueLabel.implicitWidth + Tokens.space["3"]
        implicitHeight: Tokens.space["5"] + Tokens.space["1"]

        Label {
            id: valueLabel
            anchors.centerIn: parent
            text: chip.valueText
            font.pointSize: Tokens.type.caption
            font.family: Tokens.type.monoFontFamily
            Accessible.name: chip.chipToolTip
        }

        T.ToolTip.visible: chipHover.hovered && chip.chipToolTip.length > 0
        T.ToolTip.delay: 500
        T.ToolTip.text: chip.chipToolTip
        HoverHandler { id: chipHover }
    }

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["3"]

        Label {
            Layout.fillWidth: true
            text: qsTr("Size")
            muted: true
            font.pointSize: Tokens.type.caption
            Accessible.name: text
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["1"]

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["2"]

                Label {
                    text: qsTr("Thickness")
                    muted: true
                    Accessible.name: text
                }
                Item { Layout.fillWidth: true }
                ValueChip {
                    valueText: qsTr("%1 px").arg(
                        Math.round(thicknessSlider.value))
                    chipToolTip: qsTr("Panel thickness in logical pixels")
                }
            }

            Slider {
                id: thicknessSlider

                objectName: "customizeThicknessSlider"
                Layout.fillWidth: true
                from: 20
                to: 192
                stepSize: 1
                value: root.properties.thickness ?? 32
                enabled: root.customizeSettings.canEdit
                accessibleName: qsTr("Panel thickness")
                accessibleDescription: qsTr(
                    "Panel thickness in logical pixels, 20 to 192")
                onMoved: root.customizeSettings.configureSelectedPanel(
                              "thickness", Math.round(value))
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["1"]

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space["2"]

                Label {
                    text: qsTr("Length")
                    muted: true
                    Accessible.name: text
                }
                Item { Layout.fillWidth: true }
                ValueChip {
                    valueText: qsTr("%1%").arg(
                        Math.round(lengthSlider.value * 100))
                    chipToolTip: qsTr(
                        "Share of the edge the panel occupies")
                }
            }

            Slider {
                id: lengthSlider

                objectName: "customizeLengthSlider"
                Layout.fillWidth: true
                from: 0.1
                to: 1.0
                stepSize: 0.05
                value: root.properties.length ?? 1.0
                enabled: root.customizeSettings.canEdit
                accessibleName: qsTr("Panel length")
                accessibleDescription: qsTr(
                    "Share of the edge the panel occupies, 10 to 100 percent")
                onMoved: root.customizeSettings.configureSelectedPanel(
                              "length", value)
            }
        }
    }
}
