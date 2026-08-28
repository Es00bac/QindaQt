// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
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
                description: qsTr("Theme %1").arg(themeCard.modelData.name)
                previewTokens: themeCard.modelData.previewTokens
                available: root.appearanceSettings.canEdit && !root.editorBusy
                checked: root.draftValue("appearance.theme")
                         === themeCard.modelData.id
                Accessible.description: qsTr("Select the %1 theme").arg(
                                            themeCard.modelData.name)
                onToggled: {
                    if (themeCard.checked) {
                        root.setDraft("appearance.theme", themeCard.modelData.id)
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
            editable: root.appearanceSettings.canEdit && !root.editorBusy
            descriptionPrefix: qsTr("Preferred color scheme")
            onChoicePicked: token => root.setDraft(
                                "appearance.colorScheme", token)
        }
    }
}
