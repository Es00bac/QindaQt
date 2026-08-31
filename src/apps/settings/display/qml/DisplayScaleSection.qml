// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Section for selecting UI scale factor for the active display output.
ColumnLayout {
    id: root

    required property var displaySettings
    required property bool editorBusy

    readonly property double currentScale: root.displaySettings.selectedOutput.scale ?? 1.0
    readonly property bool outputEnabled: root.displaySettings.selectedOutput.enabled ?? false

    readonly property var scalePresets: [
        { label: "100%", value: 1.0 },
        { label: "125%", value: 1.25 },
        { label: "150%", value: 1.5 },
        { label: "175%", value: 1.75 },
        { label: "200%", value: 2.0 },
        { label: "250%", value: 2.5 },
        { label: "300%", value: 3.0 }
    ]

    spacing: Tokens.space["3"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Scale & Layout")
        description: qsTr("Adjust size of text, icons, and interface elements")
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("UI Scale")
        description: qsTr("Current scale: %1%").arg(Math.round(root.currentScale * 100))
        editor: scaleChoiceRow

        Flow {
            id: scaleChoiceRow
            objectName: "displayScaleChoiceRow"
            spacing: Tokens.space["2"]

            Repeater {
                model: root.scalePresets

                delegate: Button {
                    id: scaleBtn
                    required property var modelData

                    objectName: "displayScaleButton_" + Math.round(scaleBtn.modelData.value * 100)
                    checkable: true
                    autoExclusive: true
                    available: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                    text: scaleBtn.modelData.label
                    checked: Math.abs(root.currentScale - scaleBtn.modelData.value) < 0.01

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: qsTr("Scale %1").arg(scaleBtn.modelData.label)
                    Accessible.checked: checked

                    onClicked: {
                        if (root.displaySettings.selectedOutputId) {
                            root.displaySettings.setOutputScale(
                                root.displaySettings.selectedOutputId, scaleBtn.modelData.value)
                        }
                    }
                }
            }
        }
    }
}
