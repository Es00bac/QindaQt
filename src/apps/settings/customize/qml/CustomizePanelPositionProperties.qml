// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

FormSurface {
    id: root
    required property var customizeSettings
    required property var properties

    ColumnLayout {
        width: parent.width
        spacing: Tokens.space["2"]

        FormRow {
            Layout.fillWidth: true
            label: qsTr("Position")
            description: qsTr("Move the panel to an output edge")
            editor: edgeChoices
            Flow {
                id: edgeChoices
                spacing: Tokens.space["1"]
                Repeater {
                    model: [
                        { token: "top", label: qsTr("Top") },
                        { token: "right", label: qsTr("Right") },
                        { token: "bottom", label: qsTr("Bottom") },
                        { token: "left", label: qsTr("Left") }
                    ]
                    delegate: Button {
                        required property var modelData
                        text: modelData.label
                        checkable: true
                        autoExclusive: true
                        checked: root.properties.edge === modelData.token
                        available: root.customizeSettings.canEdit
                        emphasized: checked
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: checked
                        onClicked: root.customizeSettings.configureSelectedPanel("edge", modelData.token)
                    }
                }
            }
        }

        FormRow {
            Layout.fillWidth: true
            label: qsTr("Alignment")
            editor: alignmentChoices
            Flow {
                id: alignmentChoices
                spacing: Tokens.space["1"]
                Repeater {
                    model: [
                        { token: "start", label: qsTr("Left") },
                        { token: "center", label: qsTr("Center") },
                        { token: "end", label: qsTr("Right") },
                        { token: "fill", label: qsTr("Full width") }
                    ]
                    delegate: Button {
                        required property var modelData
                        text: modelData.label
                        checkable: true
                        autoExclusive: true
                        checked: root.properties.alignment === modelData.token
                        available: root.customizeSettings.canEdit
                        emphasized: checked
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: checked
                        onClicked: root.customizeSettings.configureSelectedPanel("alignment", modelData.token)
                    }
                }
            }
        }
    }
}
