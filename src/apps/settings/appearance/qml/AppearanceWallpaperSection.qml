// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// The route model owns wallpaper validation and commit authority. This page
// only makes a desktop background choice in the shared Appearance draft.
// ADR-0286: a scope (every display, one display, one desktop, or both) picks
// where the gallery's choice applies; with no targets it is the everywhere
// wallpaper exactly as before.
ColumnLayout {
    id: root

    required property var appearanceSettings
    required property bool editorBusy
    readonly property var gallery: appearanceSettings.userWallpaperCatalog ?? null
    readonly property var draftValues: appearanceSettings.draft
    readonly property var targets: appearanceSettings.wallpaperTargets ?? null
    readonly property bool scopesAvailable: root.targets !== null
                                            && ((root.targets.displays ?? []).length > 0
                                                || (root.targets.desktops ?? []).length > 0)
    readonly property Item firstFocusTarget: root.scopesAvailable
                                             ? scopePicker.firstFocusTarget
                                             : wallpaperChoices.firstChoice
    readonly property string selectedWallpaper: String(root.draftValue("appearance.wallpaper"))
    // The scope the next pick applies to; "" means every display / desktop.
    property string selectedDisplay: ""
    property string selectedDesktop: ""
    readonly property bool everywhereScope: root.selectedDisplay === "" && root.selectedDesktop === ""
    // Re-read on every draft publication: the argument makes the draft a
    // binding dependency of the model call.
    readonly property var scopeChoice: root.choiceFor(root.draftValues, root.selectedDisplay,
                                                      root.selectedDesktop)
    // The gallery highlights only a choice this scope owns; an inherited
    // wallpaper is described by the scope picker instead.
    readonly property var pickedValue: root.everywhereScope
                                       ? root.draftValue("appearance.wallpaper")
                                       : root.scopeChoice !== null && root.scopeChoice.explicit
                                         ? root.scopeChoice.value : null

    Layout.fillWidth: true
    spacing: Tokens.space["3"]

    function draftValue(key) {
        return root.draftValues[key]
    }

    function setDraft(key, value) {
        root.appearanceSettings.setDraftValue(key, value)
    }

    function choiceFor(draft, display, desktop) {
        if (draft === undefined || root.appearanceSettings.wallpaperChoiceFor === undefined)
            return null
        return root.appearanceSettings.wallpaperChoiceFor(display, desktop)
    }

    // Every gallery pick goes through here: the everywhere scope keeps the
    // original `appearance.wallpaper` edit; any other scope is a saved choice.
    function pickWallpaper(value) {
        if (root.everywhereScope)
            root.setDraft("appearance.wallpaper", value)
        else
            root.appearanceSettings.setWallpaperFor(root.selectedDisplay, root.selectedDesktop, value)
    }

    // A display unplugged or a desktop removed while selected falls back to
    // the broader scope instead of editing something no longer listed.
    function keepScopeListed() {
        const displays = root.targets?.displays ?? []
        const desktops = root.targets?.desktops ?? []
        if (root.selectedDisplay !== "" && !displays.some(entry => entry.stableId === root.selectedDisplay))
            root.selectedDisplay = ""
        if (root.selectedDesktop !== "" && !desktops.some(entry => entry.id === root.selectedDesktop))
            root.selectedDesktop = ""
    }

    Connections {
        target: root.targets
        ignoreUnknownSignals: true
        function onDisplaysChanged() { root.keepScopeListed() }
        function onDesktopsChanged() { root.keepScopeListed() }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Wallpaper")
        description: ""
    }

    Label {
        objectName: "appearanceWallpaperAssignmentsNotice"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.appearanceSettings.wallpaperAssignmentsNotice ?? ""
        Accessible.role: Accessible.AlertMessage
    }

    WallpaperScopePicker {
        id: scopePicker
        Layout.fillWidth: true
        visible: root.scopesAvailable
        targets: root.targets
        selectedDisplay: root.selectedDisplay
        selectedDesktop: root.selectedDesktop
        choice: root.scopeChoice
        editable: root.appearanceSettings.canEdit && !root.editorBusy
        onDisplayPicked: stableId => root.selectedDisplay = stableId
        onDesktopPicked: desktopId => root.selectedDesktop = desktopId
        onClearRequested: root.appearanceSettings.clearWallpaperFor(root.selectedDisplay,
                                                                    root.selectedDesktop)
    }

    Image {
        id: wallpaperPreview
        objectName: "appearanceWallpaperPreview"
        Layout.fillWidth: true
        Layout.preferredHeight: 150
        // The model projects the draft wallpaper to a complete file URL, so
        // the preview resolves identically from any document base URL and
        // never shows a broken frame for an unknown file.
        readonly property url shown: root.everywhereScope || root.scopeChoice === null
                                     ? (root.appearanceSettings.previewWallpaper ?? "")
                                     : root.scopeChoice.previewUrl
        visible: String(wallpaperPreview.shown).length > 0
        source: wallpaperPreview.shown
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        Accessible.name: qsTr("Selected wallpaper preview")
    }

    WallpaperGallery {
        id: wallpaperChoices
        Layout.fillWidth: true
        wallpapers: (root.appearanceSettings.bundledWallpapers ?? []).concat(root.gallery?.wallpapers ?? [])
        pickedValue: root.pickedValue
        editable: root.appearanceSettings.canEdit && !root.editorBusy
        onPicked: value => root.pickWallpaper(value)
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Image file")
        description: root.everywhereScope
                     ? qsTr("Add an image to your gallery, then choose Set wallpaper to apply it")
                     : qsTr("Add an image to your gallery; it is used where Show on points")
        errorMessage: root.appearanceSettings.fieldErrors["appearance.wallpaper"] ?? ""
        editor: wallpaperField

        RowLayout {
            id: wallpaperField
            width: 320

            TextField {
                id: wallpaperPath
                objectName: "appearanceWallpaperField"
                Layout.fillWidth: true
                // A typed path edits the everywhere wallpaper only; scoped
                // choices come from the gallery and Add image….
                visible: root.everywhereScope
                enabled: root.appearanceSettings.canEdit && !root.editorBusy
                error: root.appearanceSettings.fieldErrors["appearance.wallpaper"] !== undefined
                accessibleName: qsTr("Wallpaper image file")

                // AGENT-GUARD: Two-way field that commits per keystroke. A
                // declarative text binding cannot express "follow the draft
                // except while the user is the one writing it": the qindaqt:
                // display transform clobbered the user's own typing whenever
                // the draft diverged from the display form. Keep the last
                // text this field committed and adopt only draft changes that
                // did not come from these keystrokes (bundled pick, "No
                // wallpaper", file dialog, revert, baseline rebase).
                property string lastCommitted: ""

                function displayText() {
                    return root.selectedWallpaper.startsWith("qindaqt:") ? "" : root.selectedWallpaper
                }

                function adoptDraftText() {
                    const next = displayText()
                    if (text !== next && root.selectedWallpaper !== lastCommitted) {
                        lastCommitted = next
                        text = next
                    }
                }

                Component.onCompleted: {
                    lastCommitted = displayText()
                    text = displayText()
                }
                onTextEdited: {
                    lastCommitted = text
                    root.setDraft("appearance.wallpaper", wallpaperPath.text)
                }

                Connections {
                    target: root.appearanceSettings
                    function onDraftChanged() {
                        wallpaperPath.adoptDraftText()
                    }
                }
            }
            Button {
                objectName: "appearanceChooseWallpaperButton"
                text: qsTr("Add image…")
                available: root.appearanceSettings.canEdit && !root.editorBusy
                onClicked: wallpaperDialog.open()
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Wallpaper folder")
        description: root.gallery?.folder ?? ""
        errorMessage: root.gallery?.error ?? ""
        editor: folderButtons
        RowLayout {
            id: folderButtons
            Button {
                objectName: "appearanceWallpaperFolderButton"
                text: qsTr("Choose folder…")
                available: !root.editorBusy
                onClicked: wallpaperFolderDialog.open()
            }
            Button {
                text: qsTr("Refresh")
                available: !root.editorBusy
                onClicked: root.gallery?.refresh()
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Fit")
        description: root.scopesAvailable ? qsTr("Choose how images fill every display")
                                          : qsTr("Choose how the image fills the desktop")
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

    WallpaperAssignmentList {
        Layout.fillWidth: true
        rows: root.appearanceSettings.wallpaperAssignmentRows ?? []
        editable: root.appearanceSettings.canEdit && !root.editorBusy
        onRemoveRequested: (display, desktop) => root.appearanceSettings.clearWallpaperFor(display, desktop)
    }

    FolderDialog {
        id: wallpaperFolderDialog
        title: qsTr("Choose wallpaper folder")
        onAccepted: root.gallery?.setFolder(selectedFolder)
    }

    FileDialog {
        id: wallpaperDialog
        objectName: "appearanceWallpaperDialog"
        title: qsTr("Choose wallpaper")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"), qsTr("All files (*)")]
        onAccepted: {
            const path = root.gallery?.importImage(selectedFile) ?? ""
            if (path.length > 0)
                root.pickWallpaper(path)
        }
    }
}
