// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

// Appearance route of the first-party settings application. Presentation
// only: every value read/write goes through the appearanceSettings model,
// which owns Settings1 authority, validation, and commit sequencing. This
// file must not consume the settings client or invent save/retry rules.
T.Page {
    id: root

    required property var appearanceSettings
    signal closeRequested()

    readonly property var draftValues: appearanceSettings.draft
    readonly property bool editorBusy: appearanceSettings.saving
                                     || appearanceSettings.loading
    property Item firstThemeCard: null

    title: qsTr("Appearance")

    Component.onCompleted: forceFirstThemeCardFocus()

    function forceFirstThemeCardFocus() {
        const cards = themeCardHost.children;
        for (let index = 0; index < cards.length; ++index) {
            const card = cards[index];
            if (card.objectName.startsWith("appearanceThemeCard_")) {
                root.firstThemeCard = card;
                card.forceActiveFocus(Qt.TabFocusReason);
                return;
            }
        }
    }

    function draftValue(key) {
        return root.draftValues[key];
    }

    function setDraft(key, value) {
        root.appearanceSettings.setDraftValue(key, value);
    }

    background: Rectangle {
        color: Tokens.bg.base
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.space["5"]
        spacing: Tokens.space["3"]

        Label {
            objectName: "appearancePageHeading"
            Layout.fillWidth: true
            text: qsTr("Appearance")
            font.family: Tokens.type.fontFamily
            font.pointSize: Tokens.type.title
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            objectName: "appearanceStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appearanceSettings.statusText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: root.appearanceSettings.conflict
                             || root.appearanceSettings.unavailable
                             ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text
        }

        Label {
            objectName: "appearanceError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appearanceSettings.errorText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: Tokens.status.warning.foreground
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Label {
            objectName: "appearanceThemeFallback"
            Layout.fillWidth: true
            visible: root.appearanceSettings.fallbackNotice.length > 0
            text: root.appearanceSettings.fallbackNotice
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: Tokens.fg.muted
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Flickable {
            objectName: "appearanceFormViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: formSurface.implicitHeight

            FormSurface {
                id: formSurface
                width: parent.width
                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space["2"]

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Theme")
                        description: qsTr(
                            "Choose one of the installed QindaQt themes and a preferred color scheme for new sessions")
                    }

                    Flow {
                        id: themeCardHost
                        objectName: "appearanceThemeCards"
                        Layout.fillWidth: true
                        spacing: Tokens.space["3"]

                        Repeater {
                            model: root.appearanceSettings.installedThemes

                            delegate: ThemeCard {
                                id: themeCard

                                required property var modelData

                                objectName: "appearanceThemeCard_"
                                            + themeCard.modelData.id
                                themeName: themeCard.modelData.name
                                description: qsTr("Theme %1").arg(
                                            themeCard.modelData.name)
                                previewTokens: themeCard.modelData.previewTokens
                                available: root.appearanceSettings.canEdit
                                           && !root.editorBusy
                                checked: root.draftValue("appearance.theme")
                                         === themeCard.modelData.id
                                Accessible.description: qsTr(
                                    "Select the %1 theme").arg(
                                    themeCard.modelData.name)
                                onToggled: checked => {
                                    if (checked) {
                                        root.setDraft("appearance.theme",
                                                      themeCard.modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    FormRow {
                        Layout.fillWidth: true
                        label: qsTr("Preferred color scheme")
                        description: qsTr(
                            "Used when the configured theme is unavailable; System follows the platform")
                        editor: schemeButtons

                        SegmentedChoiceRow {
                            id: schemeButtons
                            objectName: "appearanceSchemeButton"
                            choices: [
                                { token: "system", label: qsTr("System") },
                                { token: "light", label: qsTr("Light") },
                                { token: "dark", label: qsTr("Dark") }
                            ]
                            currentValue: root.draftValue("appearance.colorScheme")
                            editable: root.appearanceSettings.canEdit
                                      && !root.editorBusy
                            descriptionPrefix: qsTr("Preferred color scheme")
                            onChoicePicked: token => root.setDraft(
                                                "appearance.colorScheme", token)
                        }
                    }

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Fonts")
                        description: qsTr(
                            "Interface font preference for first-party applications")
                    }

                    FormRow {
                        Layout.fillWidth: true
                        label: qsTr("Font family")
                        description: qsTr("Font family name for interface text")
                        errorMessage: root.appearanceSettings.fieldErrors[
                                          "fonts.family"] ?? ""
                        editor: fontFamilyField

                        TextField {
                            id: fontFamilyField
                            objectName: "appearanceFontFamilyField"
                            width: 260
                            enabled: root.appearanceSettings.canEdit
                                     && !root.editorBusy
                            text: root.draftValue("fonts.family")
                            error: root.appearanceSettings.fieldErrors[
                                       "fonts.family"] !== undefined
                            accessibleName: qsTr("Font family")
                            onTextEdited: text => root.setDraft(
                                              "fonts.family", text)
                        }
                    }

                    FormRow {
                        Layout.fillWidth: true
                        label: qsTr("Font size")
                        description: qsTr("Interface font size in points")
                        errorMessage: root.appearanceSettings.fieldErrors[
                                          "fonts.pointSize"] ?? ""
                        editor: fontSizeRow

                        RowLayout {
                            id: fontSizeRow
                            spacing: Tokens.space["3"]

                            Slider {
                                id: fontSizeSlider
                                objectName: "appearanceFontSizeSlider"
                                enabled: root.appearanceSettings.canEdit
                                         && !root.editorBusy
                                from: 6.0
                                to: 36.0
                                stepSize: 0.5
                                value: Number(root.draftValue("fonts.pointSize"))
                                accessibleName: qsTr("Font size")
                                accessibleDescription: qsTr(
                                    "Interface font size in points")
                                onMoved: root.setDraft("fonts.pointSize", value)
                            }

                            Label {
                                objectName: "appearanceFontSizeValue"
                                text: fontSizeSlider.value.toFixed(1)
                                      + qsTr(" pt")
                                muted: true
                                Accessible.ignored: true
                            }
                        }
                    }

                    FormRow {
                        Layout.fillWidth: true
                        label: qsTr("Antialiasing")
                        description: qsTr(
                            "Smooth font edges for interface text")
                        editor: antialiasingSwitch

                        Switch {
                            id: antialiasingSwitch
                            objectName: "appearanceAntialiasingSwitch"
                            enabled: root.appearanceSettings.canEdit
                                     && !root.editorBusy
                            text: qsTr("Enable font antialiasing")
                            checked: root.draftValue("fonts.antialiasing") === true
                            accessibleDescription: qsTr(
                                "Antialiasing smooths font edges")
                            onToggled: checked => root.setDraft(
                                           "fonts.antialiasing", checked)
                        }
                    }

                    FormRow {
                        Layout.fillWidth: true
                        label: qsTr("Hinting")
                        description: qsTr(
                            "Grid-fitting strength for small font sizes")
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
                            editable: root.appearanceSettings.canEdit
                                      && !root.editorBusy
                            descriptionPrefix: qsTr("Font hinting")
                            onChoicePicked: token => root.setDraft(
                                                "fonts.hinting", token)
                        }
                    }

                    FormRow {
                        Layout.fillWidth: true
                        label: qsTr("Subpixel order")
                        description: qsTr(
                            "Subpixel arrangement used for text rendering")
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
                            editable: root.appearanceSettings.canEdit
                                      && !root.editorBusy
                            descriptionPrefix: qsTr("Subpixel order")
                            onChoicePicked: token => root.setDraft(
                                                "fonts.subpixelOrder", token)
                        }
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
                        description: qsTr(
                            "Leave empty for no wallpaper preference")
                        errorMessage: root.appearanceSettings.fieldErrors[
                                          "appearance.wallpaper"] ?? ""
                        editor: wallpaperField

                        TextField {
                            id: wallpaperField
                            objectName: "appearanceWallpaperField"
                            width: 320
                            enabled: root.appearanceSettings.canEdit
                                     && !root.editorBusy
                            text: root.draftValue("appearance.wallpaper")
                            error: root.appearanceSettings.fieldErrors[
                                       "appearance.wallpaper"] !== undefined
                            accessibleName: qsTr("Wallpaper image path")
                            onTextEdited: text => root.setDraft(
                                              "appearance.wallpaper", text)
                        }
                    }

                    FormRow {
                        Layout.fillWidth: true
                        label: qsTr("Wallpaper mode")
                        description: qsTr(
                            "How the wallpaper fills the desktop")
                        editor: wallpaperModeButtons

                        SegmentedChoiceRow {
                            id: wallpaperModeButtons
                            objectName: "appearanceWallpaperModeButton"
                            choices: [
                                { token: "scaled", label: qsTr("Scaled") },
                                { token: "centered", label: qsTr("Centered") },
                                { token: "tiled", label: qsTr("Tiled") }
                            ]
                            currentValue: root.draftValue(
                                              "appearance.wallpaperMode")
                            editable: root.appearanceSettings.canEdit
                                      && !root.editorBusy
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
                        description: qsTr(
                            "Applied later through the display settings boundary")
                        errorMessage: root.appearanceSettings.fieldErrors[
                                          "appearance.uiScale"] ?? ""
                        editor: uiScaleRow

                        RowLayout {
                            id: uiScaleRow
                            spacing: Tokens.space["3"]

                            Slider {
                                id: uiScaleSlider
                                objectName: "appearanceUiScaleSlider"
                                enabled: root.appearanceSettings.canEdit
                                         && !root.editorBusy
                                from: 0.5
                                to: 3.0
                                stepSize: 0.25
                                value: Number(root.draftValue("appearance.uiScale"))
                                accessibleName: qsTr("Logical UI scale")
                                accessibleDescription: qsTr(
                                    "Stored logical UI scale intent between 0.5 and 3.0")
                                onMoved: root.setDraft(
                                             "appearance.uiScale", value)
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
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.space["2"]

            Item { Layout.fillWidth: true }

            Button {
                id: retryButton
                objectName: "appearanceRetryButton"
                visible: root.appearanceSettings.unavailable
                available: !root.editorBusy
                text: qsTr("Retry")
                onClicked: root.appearanceSettings.retry()
            }

            Button {
                id: revertButton
                objectName: "appearanceRevertButton"
                visible: root.appearanceSettings.draftDirty
                         && !root.appearanceSettings.saving
                available: root.appearanceSettings.canEdit
                text: qsTr("Revert")
                onClicked: root.appearanceSettings.cancelDraft()
            }

            Button {
                id: applyButton
                objectName: "appearanceApplyButton"
                visible: !root.appearanceSettings.saving
                available: root.appearanceSettings.applyAvailable
                emphasized: true
                text: qsTr("Apply")
                onClicked: root.appearanceSettings.applyDraft()
            }

            Button {
                id: closeButton
                objectName: "appearanceCloseButton"
                available: !root.editorBusy
                text: qsTr("Close")
                KeyNavigation.tab: root.firstThemeCard
                KeyNavigation.backtab: applyButton.visible ? applyButton
                                       : revertButton.visible ? revertButton
                                       : retryButton.visible ? retryButton : applyButton
                onClicked: root.closeRequested()
            }
        }
    }
}
