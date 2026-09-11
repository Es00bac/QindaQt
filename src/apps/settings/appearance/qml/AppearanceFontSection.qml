// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Font preferences remain a shared draft here; the session bootstrap and
// running QindaQt applications consume the confirmed value after Apply.
ColumnLayout {
    id: root

    required property var appearanceSettings
    required property bool editorBusy
    readonly property var draftValues: appearanceSettings.draft
    readonly property Item firstFocusTarget: fontFamilyField
    readonly property var availableFamilies: Qt.fontFamilies()

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
        description: ""
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Font family")
        description: qsTr("Choose an installed font for interface text")
        errorMessage: root.appearanceSettings.fieldErrors["fonts.family"] ?? ""
        editor: fontFamilyField

        ComboBox {
            id: fontFamilyField
            objectName: "appearanceFontFamilyField"
            width: 260
            editable: true
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            model: root.availableFamilies
            currentIndex: root.availableFamilies.indexOf(String(root.draftValue("fonts.family")))
            editText: String(root.draftValue("fonts.family"))
            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Font family")
            Accessible.description: qsTr("Installed font families; type a family name to search")
            // Keep an in-progress typed family in the shared draft even when
            // focus moves before Enter. Selection still uses onActivated.
            onEditTextChanged: if (activeFocus)
                                   root.setDraft("fonts.family", editText)
            onActivated: index => root.setDraft("fonts.family", currentText)
            onAccepted: root.setDraft("fonts.family", editText)
        }
    }

    Label {
        objectName: "appearanceFontPreview"
        Layout.fillWidth: true
        text: qsTr("The quick brown fox jumps over the lazy dog")
        font.family: String(root.draftValue("fonts.family"))
        font.pointSize: Number(root.draftValue("fonts.pointSize"))
        wrapMode: Text.Wrap
        Accessible.name: qsTr("Font preview using %1").arg(font.family)
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
        editor: subpixelSelector

        // A selector keeps five technical values readable in a narrow form;
        // an equal-width button row overflows before the value is understood.
        ComboBox {
            id: subpixelSelector
            objectName: "appearanceSubpixelSelector"
            width: 220
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            model: [
                { value: "none", label: qsTr("None") },
                { value: "rgb", label: qsTr("RGB") },
                { value: "bgr", label: qsTr("BGR") },
                { value: "vrgb", label: qsTr("Vertical RGB") },
                { value: "vbgr", label: qsTr("Vertical BGR") }
            ]
            textRole: "label"
            currentIndex: model.findIndex(choice => choice.value === root.draftValue("fonts.subpixelOrder"))
            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Subpixel order")
            Accessible.description: qsTr("Choose the subpixel arrangement used for text rendering")
            onActivated: index => root.setDraft("fonts.subpixelOrder", model[index].value)
        }
    }
}
