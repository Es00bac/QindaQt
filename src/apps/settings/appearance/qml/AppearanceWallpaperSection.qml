// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The route model owns wallpaper validation and commit authority. This page
// only makes a desktop background choice in the shared Appearance draft.
ColumnLayout {
    id: root

    required property var appearanceSettings
    required property bool editorBusy
    readonly property var draftValues: appearanceSettings.draft
    property Item firstFocusTarget: null
    readonly property string selectedWallpaper: String(root.draftValue("appearance.wallpaper"))

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    function draftValue(key) {
        return root.draftValues[key]
    }

    function setDraft(key, value) {
        root.appearanceSettings.setDraftValue(key, value)
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Wallpaper")
        description: qsTr("Choose the desktop background and how it fits the screen")
    }

    Image {
        id: wallpaperPreview
        objectName: "appearanceWallpaperPreview"
        Layout.fillWidth: true
        Layout.preferredHeight: 150
        visible: root.selectedWallpaper.length > 0
        source: root.selectedWallpaper.startsWith("qindaqt:") ? "" : root.selectedWallpaper
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        Accessible.name: qsTr("Selected wallpaper preview")
    }

    Flow {
        Layout.fillWidth: true
        spacing: Tokens.space["2"]

        Button {
            objectName: "noWallpaperButton"
            width: 144
            height: 94
            text: qsTr("No wallpaper")
            emphasized: root.draftValue("appearance.wallpaper") === ""
            available: root.appearanceSettings.canEdit && !root.editorBusy
            onClicked: root.setDraft("appearance.wallpaper", "")
        }

        Repeater {
            model: root.appearanceSettings.bundledWallpapers ?? []

            Button {
                id: wallpaperButton
                required property var modelData
                objectName: "bundledWallpaperButton"
                width: 144
                height: 94
                text: modelData.name
                emphasized: root.draftValue("appearance.wallpaper") === modelData.value
                available: root.appearanceSettings.canEdit && !root.editorBusy
                accessibleDescription: qsTr("Select this bundled wallpaper")
                onClicked: root.setDraft("appearance.wallpaper", modelData.value)

                contentItem: ColumnLayout {
                    spacing: Tokens.space["1"]
                    Image {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        source: modelData.previewUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        Accessible.ignored: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        color: !wallpaperButton.enabled ? Tokens.fg.muted
                             : wallpaperButton.emphasized ? Tokens.accent.fg
                                                          : Tokens.fg.default
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                }
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Image file")
        description: qsTr("Choose an image from your computer, or leave it empty for no wallpaper")
        errorMessage: root.appearanceSettings.fieldErrors["appearance.wallpaper"] ?? ""
        editor: wallpaperField

        RowLayout {
            id: wallpaperField
            width: 320

            TextField {
                id: wallpaperPath
                objectName: "appearanceWallpaperField"
                Layout.fillWidth: true
                enabled: root.appearanceSettings.canEdit && !root.editorBusy
                text: root.selectedWallpaper.startsWith("qindaqt:") ? "" : root.selectedWallpaper
                error: root.appearanceSettings.fieldErrors["appearance.wallpaper"] !== undefined
                accessibleName: qsTr("Wallpaper image file")
                onTextEdited: root.setDraft("appearance.wallpaper", wallpaperPath.text)
            }
            Button {
                objectName: "appearanceChooseWallpaperButton"
                text: qsTr("Choose…")
                available: root.appearanceSettings.canEdit && !root.editorBusy
                onClicked: wallpaperDialog.open()
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Fit")
        description: qsTr("Choose how the image fills the desktop")
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
            descriptionPrefix: qsTr("Wallpaper fit")
            onChoicePicked: token => root.setDraft("appearance.wallpaperMode", token)
        }
    }

    FileDialog {
        id: wallpaperDialog
        title: qsTr("Choose wallpaper")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"), qsTr("All files (*)")]
        onAccepted: root.setDraft("appearance.wallpaper", selectedFile.toLocalFile())
    }
}
