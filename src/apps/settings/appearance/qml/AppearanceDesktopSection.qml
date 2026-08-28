// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Wallpaper and scale values are stored intent only. This component never
// reaches compositor/display APIs or mutates the running desktop.
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
        title: qsTr("Wallpaper")
        description: qsTr(
            "Stored wallpaper preference; this window does not change the running session")
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Wallpaper image path")
        description: qsTr("Leave empty for no wallpaper preference")
        errorMessage: root.appearanceSettings.fieldErrors[
                          "appearance.wallpaper"] ?? ""
        editor: wallpaperField

        TextField {
            id: wallpaperField
            objectName: "appearanceWallpaperField"
            width: 320
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            text: root.draftValue("appearance.wallpaper")
            error: root.appearanceSettings.fieldErrors[
                       "appearance.wallpaper"] !== undefined
            accessibleName: qsTr("Wallpaper image path")
            // TextInput::textEdited() has no signal argument in Qt 6.
            onTextEdited: root.setDraft("appearance.wallpaper",
                                        wallpaperField.text)
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Wallpaper mode")
        description: qsTr("How the wallpaper fills the desktop")
        editor: wallpaperModeButtons

        SegmentedChoiceRow {
            id: wallpaperModeButtons
            objectName: "appearanceWallpaperModeButton"
            choices: [
                { token: "scaled", label: qsTr("Scaled") },
                { token: "centered", label: qsTr("Centered") },
                { token: "tiled", label: qsTr("Tiled") }
            ]
            currentValue: root.draftValue("appearance.wallpaperMode")
            editable: root.appearanceSettings.canEdit && !root.editorBusy
            descriptionPrefix: qsTr("Wallpaper mode")
            onChoicePicked: token => root.setDraft(
                                "appearance.wallpaperMode", token)
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Display scale intent")
        description: qsTr(
            "Stored logical UI scale for display configuration; it is not applied by this window")
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Logical UI scale")
        description: qsTr("Applied later through the display settings boundary")
        errorMessage: root.appearanceSettings.fieldErrors[
                          "appearance.uiScale"] ?? ""
        editor: uiScaleRow

        RowLayout {
            id: uiScaleRow
            spacing: Tokens.space["3"]

            Slider {
                id: uiScaleSlider
                objectName: "appearanceUiScaleSlider"
                enabled: root.appearanceSettings.canEdit && !root.editorBusy
                from: 0.5
                to: 3.0
                stepSize: 0.25
                value: Number(root.draftValue("appearance.uiScale"))
                accessibleName: qsTr("Logical UI scale")
                accessibleDescription: qsTr(
                    "Stored logical UI scale intent between 0.5 and 3.0")
                onMoved: root.setDraft("appearance.uiScale", value)
            }

            Label {
                objectName: "appearanceUiScaleValue"
                text: Math.round(uiScaleSlider.value * 100) + "%"
                muted: true
                Accessible.ignored: true
            }
        }
    }
}
