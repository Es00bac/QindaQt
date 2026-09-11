// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0
import QindaQt.SettingsApp.Appearance 1.0

// Theme selection owns its delegate/focus lifecycle. The route model remains
// the only settings authority; this component forwards draft edits verbatim.
ColumnLayout {
    id: root

    required property var appearanceSettings
    required property bool editorBusy
    readonly property var draftValues: appearanceSettings.draft
    property Item firstThemeCard: null
    readonly property Item firstFocusTarget: firstThemeCard

    Layout.fillWidth: true
    spacing: Tokens.space["2"]

    function draftValue(key) {
        return root.draftValues[key]
    }

    function setDraft(key, value) {
        root.appearanceSettings.setDraftValue(key, value)
    }

    function focusFirstThemeCard(card) {
        if (root.firstThemeCard !== null || card === null) {
            return
        }
        root.firstThemeCard = card
        card.forceActiveFocus(Qt.TabFocusReason)
    }

    Component.onCompleted: focusFirstThemeCard(themeRepeater.itemAt(0))

    // The preview is the explanation: the previewed theme's real window
    // chrome (painted by the decoration painter the compositor uses) around
    // the real Fusion widgets ordinary Qt applications get. It follows the
    // draft, so a theme card, scheme, or font change shows before Apply.
    AppearanceWindowPreview {
        id: windowPreview
        objectName: "appearanceWindowPreview"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.round(Math.min(340, Math.max(220, width * 0.56)))
        // A duck-typed model without the preview projections (navigation
        // harnesses) leaves the item at its defaults instead of warning.
        chrome: root.appearanceSettings.previewChrome ?? ({})
        toolkitPalette: root.appearanceSettings.previewToolkitPalette ?? ({})
        toolkitFont: root.appearanceSettings.previewToolkitFont
                     ?? Qt.font({ family: Tokens.type.fontFamily, pointSize: Tokens.type.body })
        canvas: root.appearanceSettings.previewCanvasColor ?? Tokens.bg.base
        caption: qsTr("QindaQt Settings")
        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("Preview of the %1 theme").arg(
                             root.appearanceSettings.resolvedThemeId)
        Accessible.description: qsTr(
            "Window title bars, buttons, and application controls as they will look")
        T.ToolTip.visible: previewHover.hovered
        T.ToolTip.delay: 600
        T.ToolTip.text: qsTr("Title bars, buttons, and application controls as they will look")
        HoverHandler { id: previewHover }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Theme")
        description: ""
    }

    Flow {
        objectName: "appearanceThemeCards"
        Layout.fillWidth: true
        spacing: Tokens.space["3"]

        Repeater {
            id: themeRepeater
            objectName: "appearanceThemeRepeater"
            model: root.appearanceSettings.installedThemes
            onItemAdded: (index, card) => {
                if (index === 0) {
                    root.focusFirstThemeCard(card)
                }
            }

            delegate: ThemeCard {
                id: themeCard

                required property var modelData

                objectName: "appearanceThemeCard_" + themeCard.modelData.id
                themeName: themeCard.modelData.name
                description: ""
                previewTokens: themeCard.modelData.previewTokens
                available: root.appearanceSettings.canEdit && !root.editorBusy
                checked: root.draftValue("appearance.theme")
                         === themeCard.modelData.id
                Accessible.description: qsTr("Select the %1 theme").arg(
                                            themeCard.modelData.name)
                onToggled: {
                    if (themeCard.checked) {
                        root.setDraft("appearance.theme", themeCard.modelData.id)
                        if (themeCard.modelData.variant === "light")
                            root.setDraft("appearance.colorScheme", "light")
                        else if (themeCard.modelData.variant === "dark"
                                 || themeCard.modelData.variant === "dusk")
                            root.setDraft("appearance.colorScheme", "dark")
                    }
                }
            }
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Color scheme")
        description: ""
        editor: schemeButtons
        T.ToolTip.visible: schemeHover.hovered
        T.ToolTip.delay: 600
        T.ToolTip.text: qsTr("Light or dark pick a matching theme; System follows the platform")
        HoverHandler { id: schemeHover }

        SegmentedChoiceRow {
            id: schemeButtons
            objectName: "appearanceSchemeButton"
            choices: [
                { token: "system", label: qsTr("System") },
                { token: "light", label: qsTr("Light") },
                { token: "dark", label: qsTr("Dark") }
            ]
            currentValue: root.draftValue("appearance.colorScheme")
            editable: root.appearanceSettings.canEdit && !root.editorBusy
            descriptionPrefix: qsTr("Preferred color scheme")
            onChoicePicked: token => root.setDraft(
                                "appearance.colorScheme", token)
        }
    }

    // One theme choice, two consumers: QindaQt surfaces paint from QST
    // tokens, and ordinary Qt applications receive the same colors through
    // the Qt platform theme (ADR-0115). The swatches are that palette; each
    // names its role on hover.
    FormSurface {
        objectName: "appearanceQtToolkitCard"
        Layout.fillWidth: true

        RowLayout {
            width: parent.width
            spacing: Tokens.space["3"]

            Label {
                text: qsTr("Colors")
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
                Accessible.description: qsTr(
                    "The palette ordinary Qt applications receive for this theme")
            }

            Flow {
                objectName: "appearanceQtPaletteSwatches"
                Layout.fillWidth: true
                spacing: Tokens.space["2"]

                Repeater {
                    model: root.appearanceSettings.previewQtPalette

                    delegate: Rectangle {
                        id: swatch

                        required property var modelData

                        readonly property color swatchColor: modelData.color ?? Tokens.bg.base

                        width: 44
                        height: 28
                        radius: Tokens.radius.s
                        color: swatch.swatchColor
                        border.width: Tokens.space["1"] / 2
                        border.color: Tokens.outline.strong
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTr("%1: %2").arg(
                            swatch.modelData.role ?? "").arg(
                            String(swatch.swatchColor))
                        T.ToolTip.visible: swatchHover.hovered
                        T.ToolTip.delay: 500
                        T.ToolTip.text: (swatch.modelData.role ?? "")
                                        + " · " + swatch.swatchColor
                        HoverHandler { id: swatchHover }
                    }
                }
            }
        }
    }
}
