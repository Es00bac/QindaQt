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
    property var windowDecorationSettings: null
    required property bool editorBusy
    property bool detailsOpen: false
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
        wallpaper: root.appearanceSettings.previewWallpaper ?? ""
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
        // AGENT-GUARD: Flow does not contribute its laid-out children to a
        // ColumnLayout unless it publishes a height. A zero-height positioner
        // left only the first overflowing card visible/clickable in the real
        // Settings viewport even though the catalog contained every theme.
        Layout.preferredHeight: childrenRect.height
        Layout.minimumHeight: childrenRect.height
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

            // Rendered thumbnails (ADR-0206): each card paints the theme's
            // real chrome over the draft wallpaper instead of swatches.
            delegate: ThemeThumbnailCard {
                id: themeCard

                required property var modelData

                objectName: "appearanceThemeCard_" + themeCard.modelData.id
                themeName: themeCard.modelData.name
                description: qsTr("Select the %1 theme").arg(themeCard.modelData.name)
                previewTokens: themeCard.modelData.previewTokens ?? null
                previewChrome: themeCard.modelData.previewChrome ?? ({})
                wallpaper: root.appearanceSettings.previewWallpaper ?? ""
                available: root.appearanceSettings.canEdit && !root.editorBusy
                checked: root.draftValue("appearance.theme")
                         === themeCard.modelData.id
                Accessible.description: qsTr("Select the %1 theme").arg(
                                            themeCard.modelData.name)
                onToggled: {
                    if (themeCard.checked)
                        root.appearanceSettings.selectTheme(themeCard.modelData.id)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("A theme includes its color scheme, window and container decorations, title bar buttons, and their arrangement.")
        wrapMode: Text.Wrap
        muted: true
        Accessible.name: text
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Icons")
        description: ""
        errorMessage: root.appearanceSettings.fieldErrors["appearance.iconTheme"] ?? ""
        editor: iconThemeChoice
        ComboBox {
            id: iconThemeChoice
            objectName: "appearanceIconThemeChoice"
            model: root.appearanceSettings.installedIconThemes ?? [{id: "", name: qsTr("Follow theme")}]
            textRole: "name"
            valueRole: "id"
            currentIndex: {
                const selected = root.draftValue("appearance.iconTheme") ?? ""
                for (let i = 0; i < model.length; ++i)
                    if (model[i].id === selected) return i
                return 0
            }
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            onActivated: root.setDraft("appearance.iconTheme", currentValue)
            Accessible.name: qsTr("Icon theme")
        }
    }

    Button {
        objectName: "appearanceThemeDetailsButton"
        text: root.detailsOpen ? qsTr("Hide theme details") : qsTr("Fine tune this theme")
        emphasized: false
        accessibleDescription: qsTr("Customize the theme's window and container details")
        onClicked: root.detailsOpen = !root.detailsOpen
    }

    FormRow {
        Layout.fillWidth: true
        visible: root.detailsOpen
        label: qsTr("Color scheme preference")
        description: ""
        editor: schemeButtons

        SegmentedChoiceRow {
            id: schemeButtons
            objectName: "appearanceSchemeButton"
            choices: [
                { token: "system", label: qsTr("System") },
                { token: "light", label: qsTr("Light") },
                { token: "dark", label: qsTr("Dark") }
            ]
            currentValue: root.draftValue("appearance.colorScheme") ?? "system"
            editable: root.appearanceSettings.canEdit && !root.editorBusy
            descriptionPrefix: qsTr("Preferred color scheme override")
            onChoicePicked: token => root.setDraft("appearance.colorScheme", token)
        }
    }

    Loader {
        objectName: "appearanceThemeDetails"
        Layout.fillWidth: true
        active: root.detailsOpen
        sourceComponent: AppearanceWindowsSection {
            appearanceSettings: root.appearanceSettings
            windowDecorationSettings: root.windowDecorationSettings
            editorBusy: root.editorBusy
        }
    }

    // Translucency and motion are accessibility switches (ADR-0206) offered
    // here beside the materials they govern; the preview follows the draft.
    FormRow {
        Layout.fillWidth: true
        label: qsTr("Translucency")
        description: ""
        editor: translucencySwitch
        T.ToolTip.visible: translucencyHover.hovered
        T.ToolTip.delay: 600
        T.ToolTip.text: qsTr("Frosted panels, menus and title bars; off paints every surface solid")
        HoverHandler { id: translucencyHover }

        Switch {
            id: translucencySwitch
            objectName: "appearanceTranslucencySwitch"
            checked: !(root.draftValue("accessibility.reducedTransparency") ?? false)
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            accessibleDescription: qsTr("Translucent surfaces on or off")
            onToggled: root.setDraft("accessibility.reducedTransparency", !checked)
        }
    }

    FormRow {
        Layout.fillWidth: true
        label: qsTr("Motion")
        description: ""
        editor: motionSwitch
        T.ToolTip.visible: motionHover.hovered
        T.ToolTip.delay: 600
        T.ToolTip.text: qsTr("Animated menus, popups and roll-ups; off keeps every transition instant")
        HoverHandler { id: motionHover }

        Switch {
            id: motionSwitch
            objectName: "appearanceMotionSwitch"
            checked: !(root.draftValue("accessibility.reducedMotion") ?? false)
            enabled: root.appearanceSettings.canEdit && !root.editorBusy
            accessibleDescription: qsTr("Interface motion on or off")
            onToggled: root.setDraft("accessibility.reducedMotion", !checked)
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
