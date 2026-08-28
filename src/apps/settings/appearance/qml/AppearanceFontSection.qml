// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Font preferences are stored intent. Discovery and live font application
// belong to later platform consumers, never this presentation component.
ColumnLayout {
    id: root

    required property var appearanceSettings
    required property bool editorBusy
    readonly property var draftValues: appearanceSettings.draft

    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    function draftValue(key) {
        return root.draftValues[key]
    }

    function setDraft(key, value) {
        root.appearanceSettings.setDraftValue(key, value)
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Fonts")
        description: qsTr("Interface font preference for first-party applications")
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Font family")
        description: qsTr("Font family name for interface text")
        errorMessage: root.appearanceSettings.fieldErrors["fonts.family"] ?? ""
        editor: fontFamilyField

        TextField {
            id: fontFamilyField
            objectName: "appearanceFontFamilyField"
            width: 260
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            text: root.draftValue("fonts.family")
            error: root.appearanceSettings.fieldErrors["fonts.family"]
                   !== undefined
            accessibleName: qsTr("Font family")
            // TextInput::textEdited() has no signal argument in Qt 6.
            onTextEdited: root.setDraft("fonts.family", fontFamilyField.text)
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Font size")
        description: qsTr("Interface font size in points")
        errorMessage: root.appearanceSettings.fieldErrors["fonts.pointSize"] ?? ""
        editor: fontSizeRow

        RowLayout {
            id: fontSizeRow
            spacing: Tokens.space["3"]

            Slider {
                id: fontSizeSlider
                objectName: "appearanceFontSizeSlider"
                enabled: root.appearanceSettings.canEdit && !root.editorBusy
                from: 6.0
                to: 36.0
                stepSize: 0.5
                value: Number(root.draftValue("fonts.pointSize"))
                accessibleName: qsTr("Font size")
                accessibleDescription: qsTr("Interface font size in points")
                onMoved: root.setDraft("fonts.pointSize", value)
            }

            Label {
                objectName: "appearanceFontSizeValue"
                text: fontSizeSlider.value.toFixed(1) + qsTr(" pt")
                muted: true
                Accessible.ignored: true
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Antialiasing")
        description: qsTr("Smooth font edges for interface text")
        editor: antialiasingSwitch

        Switch {
            id: antialiasingSwitch
            objectName: "appearanceAntialiasingSwitch"
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            text: qsTr("Enable font antialiasing")
            checked: root.draftValue("fonts.antialiasing") === true
            accessibleDescription: qsTr("Antialiasing smooths font edges")
            onToggled: root.setDraft("fonts.antialiasing",
                                     antialiasingSwitch.checked)
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Hinting")
        description: qsTr("Grid-fitting strength for small font sizes")
        editor: hintingButtons

        SegmentedChoiceRow {
            id: hintingButtons
            objectName: "appearanceHintingButton"
            choices: [
                { token: "none", label: qsTr("None") },
                { token: "slight", label: qsTr("Slight") },
                { token: "medium", label: qsTr("Medium") },
                { token: "full", label: qsTr("Full") }
            ]
            currentValue: root.draftValue("fonts.hinting")
            editable: root.appearanceSettings.canEdit && !root.editorBusy
            descriptionPrefix: qsTr("Font hinting")
            onChoicePicked: token => root.setDraft("fonts.hinting", token)
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Subpixel order")
        description: qsTr("Subpixel arrangement used for text rendering")
        editor: subpixelButtons

        SegmentedChoiceRow {
            id: subpixelButtons
            objectName: "appearanceSubpixelButton"
            choices: [
                { token: "none", label: qsTr("None") },
                { token: "rgb", label: qsTr("RGB") },
                { token: "bgr", label: qsTr("BGR") },
                { token: "vrgb", label: qsTr("V-RGB") },
                { token: "vbgr", label: qsTr("V-BGR") }
            ]
            currentValue: root.draftValue("fonts.subpixelOrder")
            editable: root.appearanceSettings.canEdit && !root.editorBusy
            descriptionPrefix: qsTr("Subpixel order")
            onChoicePicked: token => root.setDraft("fonts.subpixelOrder", token)
        }
    }
}
