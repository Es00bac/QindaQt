// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as T
import QtQuick.Layouts
import QindaQt.Controls 1.0
import QindaQt.Tokens 1.0

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

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Theme")
        description: qsTr(
            "Choose one of the installed QindaQt themes and a preferred color scheme for new sessions")
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
        label: qsTr("Preferred color scheme")
        description: qsTr(
            "Selects a compatible theme; System follows the platform")
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
            editable: root.appearanceSettings.canEdit && !root.editorBusy
            descriptionPrefix: qsTr("Preferred color scheme")
            onChoicePicked: token => root.setDraft(
                                "appearance.colorScheme", token)
        }
    }

    // One theme choice, two consumers: QindaQt surfaces paint from QST
    // tokens, and ordinary Qt applications receive the same colors, fonts,
    // and icons through the Qt platform theme (ADR-0115). The swatches are
    // the actual palette those applications get for the previewed theme.
    FormSurface {
        objectName: "appearanceQtToolkitCard"
        Layout.fillWidth: true

        ColumnLayout {
            width: parent.width
            spacing: Tokens.space["2"]

            Label {
                Layout.fillWidth: true
                text: qsTr("Applications and toolkits")
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                muted: true
                text: qsTr("QindaQt surfaces follow QST tokens; ordinary Qt applications receive the same theme through the Qt platform theme — palette, fonts, and icons update live.")
                wrapMode: Text.Wrap
                Accessible.name: text
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

                        readonly property color swatchColor: modelData.color ?? "#000000"

                        width: 92
                        height: 56
                        radius: Tokens.radius.s
                        color: swatch.swatchColor
                        border.width: Tokens.space["1"] / 2
                        border.color: Tokens.outline.strong
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTr("%1: %2").arg(
                            swatch.modelData.role ?? "",
                            swatch.swatchColor)
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
