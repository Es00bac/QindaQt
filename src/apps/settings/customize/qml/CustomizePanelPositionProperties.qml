// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Panel position editor: a compass for the output edge and glyph buttons
// for alignment. Every control is one gesture with a tooltip — no wrapped
// text button rows that stop fitting in a narrow inspector column.
FormSurface {
    id: root

    required property var customizeSettings
    required property var properties

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["1"]

        Label {
            Layout.fillWidth: true
            text: root.properties.name ?? qsTr("Panel")
            font.weight: Font.DemiBold
            font.pointSize: Tokens.type.subtitle
            elide: Text.ElideRight
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Edge")
            muted: true
            font.pointSize: Tokens.type.caption
            Accessible.name: text
        }

        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 3
            rowSpacing: Tokens.space["1"]
            columnSpacing: Tokens.space["1"]

            Item { Layout.preferredWidth: 1; Layout.preferredHeight: 1 }

            CustomizeIconButton {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                iconName: "go-up"
                toolTip: qsTr("Move to the top edge")
                available: root.customizeSettings.canEdit
                emphasized: root.properties.edge === "top"
                onClicked: root.customizeSettings.configureSelectedPanel(
                               "edge", "top")
                Accessible.role: Accessible.RadioButton
                Accessible.checked: root.properties.edge === "top"
            }

            Item { Layout.preferredWidth: 1; Layout.preferredHeight: 1 }

            CustomizeIconButton {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                iconName: "go-previous"
                toolTip: qsTr("Move to the left edge")
                available: root.customizeSettings.canEdit
                emphasized: root.properties.edge === "left"
                onClicked: root.customizeSettings.configureSelectedPanel(
                               "edge", "left")
                Accessible.role: Accessible.RadioButton
                Accessible.checked: root.properties.edge === "left"
            }

            Item { Layout.preferredWidth: 1; Layout.preferredHeight: 1 }

            CustomizeIconButton {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                iconName: "go-next"
                toolTip: qsTr("Move to the right edge")
                available: root.customizeSettings.canEdit
                emphasized: root.properties.edge === "right"
                onClicked: root.customizeSettings.configureSelectedPanel(
                               "edge", "right")
                Accessible.role: Accessible.RadioButton
                Accessible.checked: root.properties.edge === "right"
            }

            Item { Layout.preferredWidth: 1; Layout.preferredHeight: 1 }

            CustomizeIconButton {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                iconName: "go-down"
                toolTip: qsTr("Move to the bottom edge")
                available: root.customizeSettings.canEdit
                emphasized: root.properties.edge === "bottom"
                onClicked: root.customizeSettings.configureSelectedPanel(
                               "edge", "bottom")
                Accessible.role: Accessible.RadioButton
                Accessible.checked: root.properties.edge === "bottom"
            }

            Item { Layout.preferredWidth: 1; Layout.preferredHeight: 1 }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Alignment")
            muted: true
            font.pointSize: Tokens.type.caption
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: Tokens.space["1"]

            Repeater {
                model: [
                    { token: "start", glyph: "align-start", tip: qsTr("Align to the leading edge") },
                    { token: "center", glyph: "align-center", tip: qsTr("Center the panel") },
                    { token: "end", glyph: "align-end", tip: qsTr("Align to the trailing edge") },
                    { token: "fill", glyph: "align-fill", tip: qsTr("Span the full edge") }
                ]

                delegate: CustomizeIconButton {
                    required property var modelData

                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    glyphName: modelData.glyph
                    toolTip: modelData.tip
                    available: root.customizeSettings.canEdit
                    emphasized: root.properties.alignment === modelData.token
                    onClicked: root.customizeSettings.configureSelectedPanel(
                                   "alignment", modelData.token)
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: root.properties.alignment
                                       === modelData.token
                }
            }
        }
    }
}
