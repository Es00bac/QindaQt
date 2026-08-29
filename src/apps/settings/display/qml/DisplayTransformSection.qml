// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Section for selecting display orientation and rotation.
ColumnLayout {
    id: root

    required property var displaySettings
    required property bool editorBusy

    readonly property string currentTransform: root.displaySettings.selectedOutput.transform ?? "normal"
    readonly property bool outputEnabled: root.displaySettings.selectedOutput.enabled ?? false

    readonly property var orientationPresets: [
        { label: qsTr("Standard (Landscape)"), token: "normal" },
        { label: qsTr("90° (Portrait)"), token: "90" },
        { label: qsTr("180° (Inverted)"), token: "180" },
        { label: qsTr("270° (Inverted Portrait)"), token: "270" }
    ]

    spacing: Tokens.space["3"]

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Orientation")
        description: qsTr("Rotate display content for landscape or portrait mounting")
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Rotation")
        description: qsTr("Display orientation")
        editor: orientationChoiceRow

        Flow {
            id: orientationChoiceRow
            objectName: "displayOrientationChoiceRow"
            spacing: Tokens.space["2"]

            Repeater {
                model: root.orientationPresets

                delegate: Button {
                    id: orientBtn
                    required property var modelData

                    objectName: "displayOrientationButton_" + orientBtn.modelData.token
                    checkable: true
                    autoExclusive: true
                    available: root.displaySettings.canEdit && !root.editorBusy && root.outputEnabled
                    text: orientBtn.modelData.label
                    checked: root.currentTransform === orientBtn.modelData.token

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: orientBtn.modelData.label
                    Accessible.checked: checked

                    onClicked: {
                        if (root.displaySettings.selectedOutputId) {
                            root.displaySettings.setOutputTransform(
                                root.displaySettings.selectedOutputId, orientBtn.modelData.token)
                        }
                    }
                }
            }
        }
    }
}
